#include "rocksdb/utilities/transaction_db.h"
#include "rocksdb/utilities/checkpoint.h"
#include "rocksdb/table.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/slice.h"
#include "rocksdb/iostats_context.h"
#include "rocksdb/perf_context.h"
#include <gflags/gflags.h>
#include "lmdb.h"
#include <iostream>
#include <thread>
#include <random>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

#define MCHECK(test, msg) ((test) ? (void)0 : ((void)fprintf(stderr, \
    "%s:%d: %s: %s\n", __FILE__, __LINE__, msg, mdb_strerror(rc)), abort()))
#define RES(err, expr) ((rc = expr) == (err) || (MCHECK(!rc, #expr), 0))
#define E(expr) MCHECK((rc = (expr)) == MDB_SUCCESS, #expr)

DEFINE_string(test_mode, "", "test module");
DEFINE_int64(total_kv_number, 10000000, "total kv number");
DEFINE_int64(value_size, 128, "value size");
DEFINE_int64(thread_num, 10, "thread num");
DEFINE_bool(batch_insert, false, "batch insert");
DEFINE_int64(batch_insert_num, 0, "batch insert num");
DEFINE_int64(point_read_num, 0, "point read number");
DEFINE_int64(random_test_num, 0, "random test num");
DEFINE_bool(point_read_only, false, "point read only");
DEFINE_bool(point_write_only, false, "point write only");
DEFINE_bool(point_read_write, false, "point read write");
DEFINE_bool(iter_range_scan, false, "iter range scan");
DEFINE_bool(read_write_scan, false, "read write and scan");
DEFINE_int64(iter_range_scan_num, 0, "iter range scan num");
DEFINE_bool(warm_up, false, "warm up");

using namespace std::chrono;
const std::string chars = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789";
class RandomNumberGenerator {
  std::random_device rd;
  std::mt19937_64 gen;
  std::uniform_int_distribution<uint32_t> dis;
 public:
  RandomNumberGenerator():gen(rd()) {
  }
  uint32_t next() {
    return dis(gen);
  }
};
std::string random_name() {
  std::string name = "";
  RandomNumberGenerator rng;
  for (int64_t i = 0; i < FLAGS_value_size; i++) {
    name += chars[rng.next() % chars.size()];
  }
  return name;
};

std::string const_value;

void single_kv_random_read_write(rocksdb::TransactionDB* db, MDB_env *env, MDB_dbi dbi) {
  auto rocksdb_single_update = [&]() {
    RandomNumberGenerator rng;
    rocksdb::ReadOptions ro;
    ro.verify_checksums = false;
    ro.ignore_range_deletions = true;
    rocksdb::WriteOptions wo;
    rocksdb::TransactionOptions to;
    rocksdb::Status s;
    rocksdb::PinnableSlice pinnable_val;
    for (int64_t i = 0; i <= FLAGS_random_test_num; i++) {
      if (FLAGS_point_write_only) {
        rocksdb::Transaction* txn = db->BeginTransaction(wo, to);
        s = txn->Put(std::to_string(rng.next() % FLAGS_total_kv_number),
                     const_value);
        assert(s.ok());
        s = txn->Commit();
        assert(s.ok());
        delete txn;
      } else if (FLAGS_point_read_only) {
        pinnable_val.Reset();
        const auto& k = std::to_string(rng.next() % FLAGS_total_kv_number);
        s = db->Get(ro, db->DefaultColumnFamily(), k, &pinnable_val);
        assert(s.ok());
      } else if (FLAGS_point_read_write) {
        rocksdb::Transaction* txn = db->BeginTransaction(wo, to);
        const auto& k = std::to_string(rng.next() % FLAGS_total_kv_number);
        pinnable_val.Reset();
        s = txn->Get(ro, db->DefaultColumnFamily(), k, &pinnable_val);
        assert(s.ok());
        s = txn->Put(k, const_value);
        assert(s.ok());
        s = txn->Commit();
        assert(s.ok());
        delete txn;
      } else if (FLAGS_read_write_scan) {
        rocksdb::Transaction* txn = db->BeginTransaction(wo, to);
        const auto& k = std::to_string(rng.next() % FLAGS_total_kv_number);
        auto iter = txn->GetIterator(ro);
        iter->Seek(k);
        size_t count = FLAGS_iter_range_scan_num;
        assert(iter->Valid());
        while (count > 0 && iter->Valid()) {
          iter->Next();
          count--;
        }
        delete iter;
        s = txn->Put(k, const_value);
        assert(s.ok());
        s = txn->Commit();
        assert(s.ok());
        delete txn;
      } else if (FLAGS_iter_range_scan) {
        const auto& k = std::to_string(rng.next() % FLAGS_total_kv_number);
        auto iter = db->NewIterator(ro);
        iter->Seek(k);
        size_t count = FLAGS_iter_range_scan_num;
        assert(iter->Valid());
        while (count > 0 && iter->Valid()) {
          iter->Next();
          count--;
        }
        delete iter;
      }
    }
  };
  auto lmdb_single_update = [&]() {
    int rc;
    RandomNumberGenerator rng;
    for (int64_t i = 0; i <= FLAGS_random_test_num; i++) {
      if (FLAGS_point_write_only) {
        std::string key = std::to_string(rng.next() % FLAGS_total_kv_number);
        //std::string value = random_name();
        MDB_txn* txn;
        E(mdb_txn_begin(env, NULL, MDB_NOSYNC, &txn));
        MDB_val k = {key.size(), (void*)key.data()};
        MDB_val v = {const_value.size(), nullptr};
        E(mdb_put(txn, dbi, &k, &v, MDB_RESERVE));
        memcpy((char*)(v.mv_data), const_value.data(), const_value.size());
        E(mdb_txn_commit(txn));
      } else if (FLAGS_point_read_only) {
        std::string key = std::to_string(rng.next() % FLAGS_total_kv_number);
        MDB_txn* txn;
        MDB_val k = {key.size(), (void*)key.data()};
        MDB_val v;
        E(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
        E(mdb_get(txn, dbi, &k, &v));
        mdb_txn_abort(txn);
      } else if (FLAGS_point_read_write) {
        std::string key = std::to_string(rng.next() % FLAGS_total_kv_number);
        //std::string value = random_name();
        MDB_txn* txn;
        MDB_val k = {key.size(), (void*)key.data()};
        MDB_val v;
        E(mdb_txn_begin(env, NULL, MDB_NOSYNC, &txn));
        E(mdb_get(txn, dbi, &k, &v));
        v = {const_value.size(), nullptr};
        E(mdb_put(txn, dbi, &k, &v, MDB_RESERVE));
        memcpy((char*)(v.mv_data), const_value.data(), const_value.size());
        E(mdb_txn_commit(txn));
      } else if (FLAGS_read_write_scan) {
        std::string key = std::to_string(rng.next() % FLAGS_total_kv_number);
        //std::string value = random_name();
        MDB_val k = {key.size(), (void*)key.data()};
        MDB_val v;
        MDB_cursor* cursor;
        MDB_txn* txn;
        E(mdb_txn_begin(env, NULL, MDB_NOSYNC, &txn));
        E(mdb_cursor_open(txn, dbi, &cursor));
        E(mdb_cursor_get(cursor, &k, &v, MDB_SET_KEY));
        size_t count = fLI64::FLAGS_iter_range_scan_num;
        while (count > 0 && mdb_cursor_get(cursor, &k, &v, MDB_NEXT) == 0) {
          count--;
        }
        mdb_cursor_close(cursor);
        k = {key.size(), (void*)key.data()};
        v = {const_value.size(), nullptr};
        E(mdb_put(txn, dbi, &k, &v, MDB_RESERVE));
        memcpy((char*)(v.mv_data), const_value.data(), const_value.size());
        mdb_txn_commit(txn);
      } else if (FLAGS_iter_range_scan) {
        std::string key = std::to_string(rng.next() % FLAGS_total_kv_number);
        MDB_val k = {key.size(), (void*)key.data()};
        MDB_val v;
        MDB_cursor* cursor;
        MDB_txn* txn;
        E(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
        E(mdb_cursor_open(txn, dbi, &cursor));
        E(mdb_cursor_get(cursor, &k, &v, MDB_SET_KEY));
        size_t count = fLI64::FLAGS_iter_range_scan_num;
        while (count > 0 && mdb_cursor_get(cursor, &k, &v, MDB_NEXT) == 0) {
          count--;
        }
        mdb_cursor_close(cursor);
        mdb_txn_abort(txn);
      }
    }
  };

  auto t1 = steady_clock::now();
  std::vector<std::thread> threads;
  for (int i = 0; i < FLAGS_thread_num; i++) {
    if (db)
      threads.emplace_back(std::thread(rocksdb_single_update));
    else
      threads.emplace_back(std::thread(lmdb_single_update));
  }
  for (auto &t : threads) {
    t.join();
  }
  auto t2 = steady_clock::now();
  std::cout << (db ? "===== rocksdb single_kv_random_update : " : "===== lmdb single_kv_random_update : ")
            << duration_cast<nanoseconds>(t2 - t1).count()/1000000
            << " ms "
            << std::endl << std::flush;
}

void warm_up(rocksdb::TransactionDB* db, MDB_env *env, MDB_dbi dbi) {
  if (db) {
    rocksdb::ReadOptions ro;
    ro.verify_checksums = false;
    ro.ignore_range_deletions = true;
    rocksdb::Status s;
    //s = db->CompactRange(rocksdb::CompactRangeOptions(), nullptr, nullptr);
    //assert(s.ok());
    rocksdb::PinnableSlice pinnable_val;
    for (int64_t i = 0; i < FLAGS_total_kv_number; i++) {
      pinnable_val.Reset();
      s = db->Get(ro, db->DefaultColumnFamily(), std::to_string(i),
                  &pinnable_val);
      assert(s.ok());
    }
  }

  if (env) {
    int rc;
    MDB_txn* txn;
    MDB_cursor* cursor;
    E(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
    E(mdb_cursor_open(txn, dbi, &cursor));
    for (int64_t j = 0; j < FLAGS_total_kv_number; j++) {
      const auto& k = std::to_string(j);
      MDB_val key = {k.size(), (void*)k.data()};
      MDB_val val;
      E(mdb_cursor_get(cursor, &key, &val, MDB_SET_KEY));
    }
    mdb_cursor_close(cursor);
    mdb_txn_abort(txn);
  }
}

void prepare_data(rocksdb::TransactionDB* db, MDB_env *env, MDB_dbi dbi) {
  auto rocksdb_single_write = [&](uint64_t start, uint64_t end) {
    rocksdb::WriteOptions wo;
    rocksdb::TransactionOptions to;
    rocksdb::Status s;
    for (uint64_t i = start; i <= end; i++) {
      rocksdb::Transaction* txn = db->BeginTransaction(wo, to);
      s = txn->Put(std::to_string(i), const_value);
      assert(s.ok());
      s = txn->Commit();
      delete txn;
      assert(s.ok());
    }
  };
  auto lmdb_single_write = [&](uint64_t start, uint64_t end) {
    int rc;
    for (uint64_t i = start; i <= end; i++) {
      std::string key = std::to_string(i);
      //std::string value = random_name();
      MDB_txn *txn;
      E(mdb_txn_begin(env, NULL, MDB_NOSYNC, &txn));
      MDB_val k = {key.size(), (void*)key.data()};
      MDB_val v = {const_value.size(), nullptr};
      E(mdb_put(txn, dbi, &k, &v, MDB_RESERVE));
      memcpy((char*)(v.mv_data), const_value.data(), const_value.size());
      E(mdb_txn_commit(txn));
    }
  };
  auto rocksdb_batch_write = [&](uint64_t start, uint64_t end) {
    rocksdb::WriteOptions wo;
    rocksdb::TransactionOptions to;
    rocksdb::Status s;
    for (uint64_t i = start; i <= end;) {
      rocksdb::Transaction* txn = db->BeginTransaction(wo, to);
      for (int64_t j = 0; j < FLAGS_batch_insert_num; j++) {
        s = txn->Put(std::to_string(i), const_value);
        assert(s.ok());
        i++;
        if (i > end) {
          break;
        }
      }
      s = txn->Commit();
      delete txn;
      assert(s.ok());
    }
  };
  auto lmdb_batch_write = [&](uint64_t start, uint64_t end) {
    int rc;
    for (uint64_t i = start; i <= end;) {
      MDB_txn *txn;
      E(mdb_txn_begin(env, NULL, MDB_NOSYNC, &txn));
      for (int64_t j = 0; j < FLAGS_batch_insert_num; j++) {
        std::string key = std::to_string(i);
        //std::string value = random_name();
        MDB_val k = {key.size(), (void*)key.data()};
        MDB_val v = {const_value.size(), (void*)const_value.data()};
        E(mdb_put(txn, dbi, &k, &v, MDB_NOOVERWRITE));
        i++;
        if (i > end) {
          break;
        }
      }
      E(mdb_txn_commit(txn));
    }
  };

  auto t1 = steady_clock::now();
  int64_t cut = FLAGS_total_kv_number/FLAGS_thread_num;
  std::vector<std::thread> threads;
  for (int64_t i = 0; i < FLAGS_thread_num; i++) {
    auto begin = i*cut;
    auto end =  std::min((i+1)*cut - 1, FLAGS_total_kv_number);
    //std:: cout << begin << "," << end << std::endl;
    if (FLAGS_batch_insert) {
      if (db)
        threads.emplace_back(std::thread(rocksdb_batch_write, begin, end));
      else
        threads.emplace_back(std::thread(lmdb_batch_write, begin, end));
    } else {
      if (db)
        threads.emplace_back(std::thread(rocksdb_single_write, begin, end));
      else
        threads.emplace_back(std::thread(lmdb_single_write, begin, end));
    }
  }
  for (auto &t : threads) {
    t.join();
  }
  auto t2 = steady_clock::now();
  std::cout << (db ? "===== rocksdb prepare_data : " : "===== lmdb prepare_data : ")
            << duration_cast<nanoseconds>(t2 - t1).count()/1000000
            << " ms "
            << std::endl << std::flush;
}

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  const_value = random_name();

  rocksdb::Options options;
  rocksdb::TransactionDBOptions txn_db_options;
  options.create_if_missing = true;
  rocksdb::BlockBasedTableOptions table_options;
  table_options.cache_index_and_filter_blocks = true;
  table_options.block_cache = rocksdb::NewLRUCache(10 * 1024 * 1024 * 1024L);
  table_options.data_block_index_type =
      rocksdb::BlockBasedTableOptions::kDataBlockBinaryAndHash;
  table_options.partition_filters = true;
  table_options.index_type =
      rocksdb::BlockBasedTableOptions::IndexType::kTwoLevelIndexSearch;
  options.table_factory.reset(
      rocksdb::NewBlockBasedTableFactory(table_options));
  options.enable_pipelined_write = true;
  options.IncreaseParallelism();
  options.OptimizeLevelStyleCompaction();
  rocksdb::TransactionDB* db;
  rocksdb::Status s;
  s = rocksdb::TransactionDB::Open(options, txn_db_options, "rocksdb_data", &db);
  assert(s.ok());

  if (!fs::exists("lmdb_data")) {
    fs::create_directories("lmdb_data");
  }
  int rc;
  MDB_env *env;
  MDB_dbi dbi;
  MDB_txn *txn;
  E(mdb_env_create(&env));
  E(mdb_env_set_mapsize(env, (size_t)1 << 40));
  E(mdb_env_set_maxdbs(env, 255));
  unsigned int flags = MDB_NOMEMINIT | MDB_NORDAHEAD;
  E(mdb_env_open(env, "lmdb_data", flags, 0664));
  E(mdb_txn_begin(env, NULL, MDB_NOSYNC, &txn));
  E(mdb_dbi_open(txn, NULL, 0, &dbi));
  E(mdb_txn_commit(txn));

  if (FLAGS_test_mode == "prepare_data") {
    std::cout << "FLAGS_thread_num: " << FLAGS_thread_num << std::endl;
    std::cout << "FLAGS_total_kv_number: " << FLAGS_total_kv_number
              << std::endl;
    std::cout << "FLAGS_value_size: " << FLAGS_value_size << std::endl;
    std::cout << "FLAGS_batch_insert: " << FLAGS_batch_insert << std::endl;
    std::cout << "FLAGS_batch_insert_num: " << FLAGS_batch_insert_num
              << std::endl;

    prepare_data(db, nullptr, 0);
    db->Close();
    delete db;

    prepare_data(nullptr, env, dbi);
    mdb_dbi_close(env, dbi);
    mdb_env_close(env);

  } else if (FLAGS_test_mode == "single_kv_random_update") {
    std::cout << "FLAGS_warm_up: " << FLAGS_warm_up << std::endl;
    std::cout << "FLAGS_total_kv_number: " << FLAGS_total_kv_number
              << std::endl;
    std::cout << "FLAGS_thread_num: " << FLAGS_thread_num << std::endl;
    std::cout << "FLAGS_random_test_num: " << FLAGS_random_test_num << std::endl;
    std::cout << "FLAGS_value_size: " << FLAGS_value_size << std::endl;
    std::cout << "FLAGS_point_write_only: " << FLAGS_point_write_only << std::endl;
    std::cout << "FLAGS_point_read_only: " << FLAGS_point_read_only << std::endl;
    std::cout << "FLAGS_point_read_write: " << FLAGS_point_read_write << std::endl;
    std::cout << "FLAGS_read_write_scan: " << FLAGS_read_write_scan << std::endl;
    std::cout << "FLAGS_iter_range_scan: " << FLAGS_iter_range_scan << std::endl;
    std::cout << "FLAGS_iter_range_scan_num: " << FLAGS_iter_range_scan_num << std::endl;

    if (FLAGS_warm_up)
      warm_up(db, nullptr, 0);
    single_kv_random_read_write(db, nullptr, 0);
    db->Close();
    delete db;

    if (FLAGS_warm_up)
      warm_up(nullptr, env, dbi);
    single_kv_random_read_write(nullptr, env, dbi);
    mdb_dbi_close(env, dbi);
    mdb_env_close(env);

  } else {
    std::cerr << "unknown test mode" << std::endl;
    db->Close();
    delete db;
    mdb_dbi_close(env, dbi);
    mdb_env_close(env);
  }

  std::cout << std::endl;
  return 0;
}
