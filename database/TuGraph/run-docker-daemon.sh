DOCKER_IMAGE_NAME=tugraph/tugraph-runtime-centos7
DOCKER_IMAGE_TAG=3.6.0
PROJECT_ROOT=/data/projects/gdbms-microbenchmark/
TWITTER_DATA_ROOT=/data/twitter/

docker run -d -p 7070:7070 \
    -v ${PROJECT_ROOT}:/root/gdbms-microbenchmark \
    -v ${TWITTER_DATA_ROOT}:/root/data/ \
    -it ${DOCKER_IMAGE_NAME}:${DOCKER_IMAGE_TAG} bash
