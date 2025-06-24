#!/bin/bash

mkdir -p Db || exit 1
cd Db || exit 1

sqlite3 chat.db <schema.sql
