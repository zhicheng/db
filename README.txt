
Simple Hash DB for C

Still Working on It.DO NOT use

Demo:
=====
if (db_open(&db, "foo.db", 0) != DB_OK) {
        fprintf(stderr, "open db failed\n");
        return 0;
}

if (db_put(&db, "hi", strlen("hi"), "hello,world", strlen("hello,world")) != DB_OK) {
        fprintf(stderr, "NOT OK\n");
}

if ((len = db_get(&db, "hi", strlen("hi"), val, sizeof(val))) == 0) {
        fprintf(stderr, "NOT FOUND\n");
}

db_close(&db);

Limited:
========

In 32 bit platform database file size is limited 4GiB

Key/value length is 32 bit unsigned int

Design:
======

                +----------+
                |          |
                |  header  |
                |          |
                +----------+
                | table[0] |---------+
                +----------+         |
                | table[1] |------------+
                +----------+         |  |
                | table[2] |         |  |
                +----------+         |  |
                |    .     |         |  |
                |    .     |         |  |
                |    .     |         |  |
                +----------+         |  |
                |table[256]|         |  |
                +----------+         |  |
          +-----| slot[0]  |<--------|  |
          |     +----------+            |
          |     | slot[1]  |            |
          |     +----------+            |
       +--------| slot[2]  |            |
       |  |     +----------+            |
       |  |     |    .     |            |
       |  |     |    .     |<-----------+
       |  |     |    .     |
       |  |     +----------+
       |  |     | slot[N]  |
       |  |     +----------+
       |  +---->| key len  |
       |        +----------+
       |        | val len  |
       |        +----------+
       |        |    .     |
       |        | key len  |
       |        |  bytes   |
       |        |    .     |
       |        +----------+
       |        |    .     |
       |        | val len  |
       |        |  bytes   |
       |        |    .     |
       |        +----------+
       +------->| key len  |
                +----------+
                | val len  |
                +----------+
                |    .     |
                | key len  |
                |  bytes   |
                |    .     |
                +----------+
                |    .     |
                | val len  |
                |  bytes   |
                |    .     |
                +----------+
                |    .     |
                |    .     |
                |    .     |
                +----------+

Public Domain License
