
Simple Hash DB for C

Still Working on It.DO NOT use

Demo:
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

Public Domain License
