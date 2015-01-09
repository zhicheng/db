Simple Hash DB for C

Next Release will has Dynamic Hash Implementation


Demo:
=====
db_t db;
db_option_t option;

option.table  = 256;
option.bucket = 256;
option.rdonly = 0;
if (db_open(&db, "foo.db", "foo.db", &option) != DB_OK) {
        fprintf(stderr, "open db failed\n");
        return 0;
}

if (db_put(&db, "hi", strlen(...), "hello,world", strlen(...)) != DB_OK) {
        fprintf(stderr, "NOT OK\n");
}

if ((len = db_get(&db, "hi", strlen(...), val, sizeof(val))) == 0) {
        fprintf(stderr, "NOT FOUND\n");
}

db_close(&db);


Limited:
========

In 32 bit platform database file size is limited 4GiB*

Key/value length is 32 bit unsigned int

*Depends Your Operation System,Mostly can't get 4GiB map


Design:
=======

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


Goal:
=====

Keep it simple, stupid


FAQ:
====

Q: Do you use `mmap'? What if I don't want use `mmap'?
A: Yes.Just use others,there is a lot of key/value database you can choose.

Q: I tried this library,It's waste to much disk space and memory!
A: I'll write compaction function later,will reduce disk space use.
   Memory is control by the kernel,Sorry.

Q: Compression?
A: Maybe.

Q: Encryption?
A: Maybe.

Q: I use this in my Web Server,I have a problem!
A: Please contact the author.

Q: I use this in my Mobile Phone,I have a problem!
A: Please contact the author.

Q: I want do X,Will be OK?
A: Just try.If don't,Please contact the author.

Q: I have a problem!
A: Please contact the author.


License:
========

Public Domain License
