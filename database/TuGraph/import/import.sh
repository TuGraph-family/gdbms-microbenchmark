#!/bin/bash
lgraph_import --dir /var/lib/lgraph/graph500_data --verbose 2 -c import.conf --dry_run 0 --continue_on_error 1 --overwrite 1
