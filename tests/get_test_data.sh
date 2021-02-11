#!/bin/bash

TEST_DIR=`dirname $0`
TEST_DATA_DIR=kernel-shark_testdata
TEST_DATA_REPO=https://github.com/yordan-karadzhov/${TEST_DATA_DIR}.git

rm -fv ${TEST_DIR}/*.dat

if [ -d "${TEST_DATA_DIR}" ]; then
    rm -rf ${TEST_DATA_DIR}
fi

git clone ${TEST_DATA_REPO}

if [ ! -d "${TEST_DATA_DIR}" ]; then
    exit false
fi

rm -f ${TEST_DATA_DIR}/LICENSE
cp -v ${TEST_DATA_DIR}/* ${TEST_DIR}/
rm -rf ${TEST_DATA_DIR}
