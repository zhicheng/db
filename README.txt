A New DBM in Pure C


Demo:
=====
db_t db;
db_option_t option;

option.table  = 256;	/* table number,keep this small if data not too much */
option.bucket = 256;    /* initialize bucket number in per table,will incrase when key add */
option.rdonly = 0;
if (db_open(&db, /* data file */ "foo.db", /* index file */ "foo.db", &option) != DB_OK) {
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

Key/Value length is 32 bit unsigned int

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
                | table[N] |         |  |
                +----------+         |  |
          +-----| bucket[0]|<--------|  |
          |     +----------+            |
          |     | bucket[1]|            |
          |     +----------+            |
       +--------| bucket[2]|            |
       |  |     +----------+            |
       |  |     |    .     |            |
       |  |     |    .     |<-----------+
       |  |     |    .     |
       |  |     +----------+
       |  |     | bucket[N]|
       |  |     +----------+
       |  +---->|   klen   |
       |        +----------+
       |        |   vlen   |
       |        +----------+
       |        |    .     |
       |        |   klen   |
       |        |  bytes   |
       |        |    .     |
       |        +----------+
       |        |    .     |
       |        |   vlen   |
       |        |  bytes   |
       |        |    .     |
       |        +----------+
       +------->|   klen   |
                +----------+
                |   vlen   |
                +----------+
                |    .     |
                |   klen   |
                |  bytes   |
                |    .     |
                +----------+
                |    .     |
                |   vlen   |
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

Next Release will has Dynamic Hash Implementation*
And Mmap Maybe not required.

*Litwin, Witold (1980), "Linear hashing: A new tool for file and table addressing"


FAQ:
====

Q: Do you use `mmap'? What if I don't want use `mmap'?
A: Yes.Just use others,there is a lot of key/value database you can choose.

Q: I tried this library,It's waste to much disk space and memory!
A: I'll write compaction function later,will reduce disk space in high update application,you can use db-export and db-import to a new database file.The future compaction function will do same thing.Memory is control by the kernel,Sorry.

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
