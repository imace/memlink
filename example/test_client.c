#include <stdio.h>
#include <string.h>

#include "memlink_client.h"
#include "logfile.h"

MemLink *m;

int test_init()
{

	logfile_create("stdout", 4);
	m = memlink_create("127.0.0.1", 11001, 11002, 3);
	if (m == NULL) {
		printf("memlink_create error!\n");
	}

	return 1;
}

//create example
int test_create_key_list()
{
	char buf[128] = {0};
	int ret;

	snprintf(buf, 128, "%s", "test");

	ret = memlink_cmd_create_list(m, buf, 6, "4:3:1");
	DINFO("memlink_cmd_crate: %d\n", ret);
	
	return 1;

}

//create sort list key
int
test_create_sortlist()
{
    int ret;
    char buf[128];

    snprintf(buf, 128, "%s", "test_sort");

    if ((ret = memlink_cmd_create_sortlist(m, buf, 6, "4:3:1", MEMLINK_VALUE_OBJ)) != MEMLINK_OK) {
        DERROR("create sortlist error: %d\n", ret);
    }

    return 1;
}

//insert example
int test_insert_key_value(char *value)
{
	char buf[128] = {0};
	int ret;
    test_create_key_list();

	snprintf(buf, 128, "%s", value);
	DINFO("INSERT test mykey %s\n", buf);

	ret = memlink_cmd_insert(m, "test", buf, strlen(buf), "1:2:0", 0);

	DINFO("memlink_cmd_insert: %d\n", ret);

	return 1;
}

//sortlist insert example
int
test_sortlist_insert()
{
    int ret;
    char buf[128];

    //this will sort
    snprintf(buf, 128, "%s", "jsortv1");
    if ((ret = memlink_cmd_sortlist_insert(m, "test_sort", buf, strlen(buf), "1:2:0")) != MEMLINK_OK) {
        DERROR("sortlist insert error: %d\n", ret);
    }

    //this will not sort
    snprintf(buf, 128, "%s", "jvalue");
    if ((ret = memlink_cmd_sortlist_insert(m, "test", buf, strlen(buf), "1:2:0")) != MEMLINK_OK) {
        DERROR("sortlist insert error: %d\n", ret);
    }

    return 1;
}

//mkv insert example
int
test_insert_mkv()
{
    MemLinkInsertMkv *mkv;
    MemLinkInsertKey *key;
    MemLinkInsertVal *val;
    int ret, i;
    char buf[20];

    mkv = memlink_imkv_create();

    //create one key
    snprintf(buf, 20, "%s", "haha");
    ret = memlink_cmd_rmkey(m, buf);
    if (ret != MEMLINK_OK) {
        DERROR("memlink_cmd_rmkey: %d\n", ret);
    }
	ret = memlink_cmd_create_list(m, buf, 6, "4:3:4");
    if (ret != MEMLINK_OK) {
        DERROR("memlink_cmd_create_list: %d\n", ret);
    }

    key = memlink_ikey_create(buf, strlen(buf));

    if ((ret = memlink_imkv_add_key(mkv, key)) < 0) {
        DERROR("mkv add key error: %d\n", ret);
        memlink_imkv_destroy(mkv);
    }

    for (i = 0; i < 100; i++) {
        snprintf(buf, 20, "%06d", i);
        val = memlink_ival_create(buf, strlen(buf), "1:2:4", 0);
        if (!val) {
            memlink_imkv_destroy(mkv);
        }
        memlink_ikey_add_value(key, val);
    }

    //create another key
    snprintf(buf, 20, "%s", "haha0");
    ret = memlink_cmd_rmkey(m, buf);
    if (ret != MEMLINK_OK) {
        DERROR("memlink_cmd_rmkey: %d\n", ret);
    }
	ret = memlink_cmd_create_list(m, buf, 6, "4:3:4");
    if (ret != MEMLINK_OK) {
        DERROR("memlink_cmd_create_list: %d\n", ret);
    }

    key = memlink_ikey_create(buf, strlen(buf));

    if ((ret = memlink_imkv_add_key(mkv, key)) < 0) {
        DERROR("mkv add key error: %d\n", ret);
         memlink_imkv_destroy(mkv);
    }

    for (i = 100; i < 200; i++) {
        snprintf(buf, 20, "%06d", i);
        val = memlink_ival_create(buf, strlen(buf), "1:2:4", 0);
        if (!val) {
            memlink_imkv_destroy(mkv);
        }
        memlink_ikey_add_value(key, val);
    }

    if ((ret = memlink_cmd_insert_mkv(m, mkv)) != MEMLINK_OK) {
        DERROR("mkv insert error: %d\n", ret);
    }
    memlink_imkv_destroy(mkv);

    return 1;
}

//stat example
int test_stat()
{
	MemLinkStat stat;
	int ret;
	
	ret = memlink_cmd_stat(m, "test", &stat);
	DINFO("memlink_cmd_stat: %d\n", ret);
	DINFO("valuesize:%d, masksize:%d, blocks:%d, data:%d, data_used:%d, mem:%d\n", stat.valuesize, stat.masksize, stat.blocks, stat.data, stat.data_used, stat.mem);

	return 1;

}

//stat sys example
int
test_stat_sys()
{
    int ret;
    MemLinkStatSys stat;

    if ((ret = memlink_cmd_stat_sys(m, &stat)) != MEMLINK_OK) {
        DERROR("stat_sys error: %d\n", ret);
    }

    DINFO("MEMLINK SYSINFO:\n\tversion: %d\n\tkeys: %u\n \
\tvalues: %u\n\tblocks: %u\n\tdata_all: %u\n \
\tht_mem: %u\n\tpool_mem: %u\n\tpool_blocks: %u\n \
\tall_mem: %u\n\tconn_read: %u\n \
\tconn_write: %u\n\tconn_sync: %u\n \
\tthreads: %u\n\tpid: %u\n \
\tuptime: %u\n\tbit: %u\n \
\tlast_dump: %u\n", \
           stat.version, stat.keys, stat.values, stat.blocks, stat.data_all, \
           stat.ht_mem, stat.pool_mem, stat.pool_blocks, stat.all_mem, \
           stat.conn_read, stat.conn_write, stat.conn_sync, stat.threads, \
           stat.pid, stat.uptime, stat.bit, stat.last_dump \
          );

    return 1;
}

//range example
int test_range()
{
	int ret;
	MemLinkResult result;
	MemLinkItem *p ;

	MemLinkStat stat;
	unsigned int count;

	if ((ret = memlink_cmd_stat(m, "test", &stat)) != MEMLINK_OK) {
		DERROR("count error: %d\n", ret);
		count = 2;
	} else {
	    count = stat.data;
        }

	ret = memlink_cmd_range(m, "test", MEMLINK_VALUE_VISIBLE, "::", 0, count, &result);
	if (ret == MEMLINK_OK) {
		DINFO("valuesize:%d, masksize:%d, count:%d\n", result.valuesize, result.masksize,
			result.count);

		p = result.root;
		while (p != NULL) {
			DINFO("value: %s, mask: %s\n", p->value, p->mask);
			p = p->next;
		}
	}
	return 1;

	memlink_result_free(&result);
}

//sortlist range
int
test_sortlist_range()
{
    int ret;
    MemLinkResult r;
    MemLinkItem *n;

    if ((ret = memlink_cmd_sortlist_range(m, "test_sort", MEMLINK_VALUE_VISIBLE, "::", "asortv", 6, "zsortv", 6, &r)) != MEMLINK_OK) {
        DERROR("sortlist range error: %d\n", ret);
        return -1;
    }

    DINFO("count: %d, valuesize: %d, masksize: %d\n", r.count, r.valuesize, r.masksize);

    n = r.root;

    while (n) {
        DINFO("value: %s, mask: %s\n", n->value, n->mask);
        n = n->next;
    }

    memlink_result_free(&r);

    return 1;
}

//rmkey example
int test_rm_key()
{
	int ret;

	ret = memlink_cmd_rmkey(m, "test");
	DINFO("memlink_cmd_rmkey:%d\n", ret);

	return 1;
}

//count example
int test_count()
{
	int ret;
	MemLinkCount count;

	ret = memlink_cmd_count(m, "test", "::", &count);

	DINFO("memlink_cmd_count: ret: %d, visible_count: %d, tagdel_count: %d\n", ret, count.visible_count, count.tagdel_count);
	return 1;
}

//sort list count example
int
test_sortlist_count()
{
    int ret;

    MemLinkCount count;

    if ((ret = memlink_cmd_sortlist_count(m, "test_sort", "::", "asortv", 6, "zsortv", 6, &count)) != MEMLINK_OK) {
        DERROR("sortlist count error: %d\n", ret);
        return -1;
    }

    DINFO("visible_count: %u, tagdel_count: %u\n", count.visible_count, count.tagdel_count);

    return 1;
}

//del example
int test_del_value()
{
	int ret;
	char key[10];
    char value[10];

	sprintf(key, "%s", "test");
	sprintf(value, "%s", "myvalue");

	ret = memlink_cmd_del(m, key, value, strlen(value));
	DINFO("memlink_cmd_del: %d", ret);
	return 1;
}

//sortlist del example
int
test_sortlist_del()
{
    int ret;

    if ((ret = memlink_cmd_sortlist_del(m, "test_sort", MEMLINK_VALUE_VISIBLE, "::", "jsortv", 6, "sortv1", 6)) != MEMLINK_OK) {
        DERROR("sortlist del error: %d\n", ret);
    }

    return 1;
}

//del by mask example
int
test_del_by_mask()
{
    int ret;

    if ((ret = memlink_cmd_del_by_mask(m, "test", "1:2:2")) != MEMLINK_OK) {
        DERROR("del by mask error: %d\n", ret);
    }

    return 1;
}

//mask example
int test_mask()
{
	int ret;
	char key[10];
	char value[10];

	sprintf(key, "%s", "test");
	sprintf(value, "%s", "myvalue");

	ret = memlink_cmd_mask(m, key, value, strlen(value), "1:1:1");
	DINFO("memlink_cmd_mask: %d\n", ret);
	return 1;
}

//dump example
int test_dump()
{
	int ret;
	
	ret = memlink_cmd_dump(m);
	if (ret != MEMLINK_OK) {
		DERROR("dump error: %d\n", ret);
	}
	return 1;
}

//ping example
int
test_ping()
{
    int ret;

    if ((ret = memlink_cmd_ping(m)) != MEMLINK_OK) {
        DERROR("ping error: %d\n", ret);
    }

    return 1;
}

//clean example
int
test_clean()
{
    int ret;

    if ((ret = memlink_cmd_clean(m, "test")) != MEMLINK_OK) {
        DERROR("clean error: %d\n", ret);
    }

    return 1;
}

//clean all example
int
test_clean_all()
{
    int ret;

    if ((ret = memlink_cmd_clean_all(m)) != MEMLINK_OK) {
        DERROR("clean all error: %d\n", ret);
    }

    return 1;
}

//move example
int
test_move()
{
    int ret;
    char buf[128];

    snprintf(buf, 128, "%s", "99");

    if ((ret = memlink_cmd_move(m, "test", buf, strlen(buf), 0)) != MEMLINK_OK) {
        DERROR("move error: %d\n", ret);
    }

    return 1;
}

//tag example
int
test_tag(int tag)
{
    int ret;
    char buf[128];

    snprintf(buf, 128, "%s", "99");

    if ((ret = memlink_cmd_tag(m, "test", buf, strlen(buf), tag)) != MEMLINK_OK) {
        DERROR("tag error: %s\n", ret);
    }

    return 1;
}

//lpush example
int
test_lpush()
{
    int ret;
    char buf[128];

    snprintf(buf, 128, "%s", "lpushvalue");

    if ((ret = memlink_cmd_lpush(m, "test", buf, strlen(buf), "1:2:0"))) {
        DERROR("lpush error: %d\n", ret);
    }

    return 1;
}

//lpop example
int
test_lpop()
{
    int ret;
    MemLinkResult r;
    MemLinkItem *n;

    if ((ret = memlink_cmd_lpop(m, "test", 2, &r))) {
        DERROR("lpush error: %d\n", ret);
        return -1;
    }

    DINFO("count: %d valuesize: %d masksize: %d\n", r.count, r.valuesize, r.masksize);

    n = r.root;
    while (n) {
        DINFO("\tvalue: %s mask: %s\n", n->value, n->mask);
        n = n->next;
    }

    memlink_result_free(&r);

    return 1;
}

//rpush example
int
test_rpush()
{
    int ret;
    char buf[128];

    snprintf(buf, 128, "%s", "rpushvalue");

    if ((ret = memlink_cmd_rpush(m, "test", buf, strlen(buf), "1:2:0"))) {
        DERROR("rpush error: %d\n", ret);
    }

    return 1;
}

//rpop example
int
test_rpop()
{
    int ret;
    MemLinkResult r;
    MemLinkItem *n;

    if ((ret = memlink_cmd_rpop(m, "test", 2, &r))) {
        DERROR("lpush error: %d\n", ret);
        return -1;
    }

    DINFO("count: %d valuesize: %d masksize: %d\n", r.count, r.valuesize, r.masksize);

    n = r.root;
    while (n) {
        DINFO("\tvalue: %s mask: %s\n", n->value, n->mask);
        n = n->next;
    }

    memlink_result_free(&r);

    return 1;
}

//read conn info example
int
test_read_conn_info()
{
    int ret, c = 1;
    MemLinkRcInfo rcinfo;
    MemLinkRcItem *n;

    if ((ret = memlink_cmd_read_conn_info(m, &rcinfo)) != MEMLINK_OK) {
        DERROR("read conn info error: %d\n", ret);
        return -1;
    }

    DINFO("conncount: %d\n", rcinfo.conncount);
    n = rcinfo.root;

    while (n) {
        DINFO("\n\tconn %d:\n"
              "\t\tfd: %d\n"
              "\t\tclient_ip: %s\n"
              "\t\tport: %d\n"
              "\t\tcmd_count: %d\n"
              "\t\tconn_time: %u\n", c, n->fd, n->client_ip, n->port, n->cmd_count, n->conn_time);
        n = n->next;
        c++;
    }


    memlink_rcinfo_free(&rcinfo);

    return 1;
}

//write conn info example
int
test_write_conn_info()
{
    int ret, c = 1;
    MemLinkWcInfo wcinfo;
    MemLinkWcItem *n;

    if ((ret = memlink_cmd_write_conn_info(m, &wcinfo)) != MEMLINK_OK) {
        DERROR("write conn info error: %d\n", ret);
        return -1;
    }

    DINFO("conncount: %d\n", wcinfo.conncount);
    n = wcinfo.root;

    while (n) {
        DINFO("\n\tconn %d:\n"
              "\t\tfd: %d\n"
              "\t\tclient_ip: %s\n"
              "\t\tport: %d\n"
              "\t\tcmd_count: %d\n"
              "\t\tconn_time: %u\n", c, n->fd, n->client_ip, n->port, n->cmd_count, n->conn_time);
        n = n->next;
        c++;
    }

    memlink_wcinfo_free(&wcinfo);

    return 1;
}

//sync conn info example
int
test_sync_conn_info()
{
    int ret, c = 1;
    MemLinkScInfo scinfo;
    MemLinkScItem *n;

    if ((ret = memlink_cmd_sync_conn_info(m, &scinfo)) != MEMLINK_OK) {
        DERROR("sync conn info error: %d\n", ret);
        return -1;
    }

    DINFO("sync conncount: %d\n", scinfo.conncount);

    n = scinfo.root;
    while (n) {
        DINFO("\nsync conn: %d\n"
              "\t\tfd: %d\n"
              "\t\tclient_ip: %s\n"
              "\t\tport: %d\n"
              "\t\tcmd_count: %d\n"
              "\t\tconn_time: %u\n"
              "\t\tlogver: %d\n"
              "\t\tlogline: %d\n"
              "\t\tdelay: %d\n",
              c, n->fd, n->client_ip, n->port, n->cmd_count, n->conn_time, n->logver, n->logline, n->delay);
        n = n->next;
        c++;
    }

    memlink_scinfo_free(&scinfo);
    return 1;
}

//get config info example
int
test_get_config_info()
{
    int ret, i;
    MyConfig conf;

    memset(&conf, 0, sizeof(conf));

    if ((ret = memlink_cmd_get_config_info(m, &conf)) != MEMLINK_OK) {
        DERROR("get config info error: %d\n", ret);
        return -1;
    }

    DINFO("The Configure:\n");
    printf("\tblock_data_count:");
    for (i = 0; i < conf.block_data_count_items; i++)
        printf(" %d", conf.block_data_count[i]);
    printf("\n");
    printf("\tblock_data_count_items: %d\n"
           "\tdump_interval: %u\n"
           "\tblock_clean_cond: %f\n"
           "\tblock_clean_start: %d\n"
           "\tblock_clean_num: %d\n"
           "\tread_port: %d\n"
           "\twrite_port: %d\n"
           "\tsync_port: %d\n"
           "\tdatadir: %s\n"
           "\tlog_level: %d\n"
           "\tlog_name: %s\n"
           "\twrite_binlog: %d\n"
           "\ttimeout: %d\n"
           "\tthread_num: %d\n"
           "\tmax_conn: %d\n"
           "\tmax_read_conn: %d\n"
           "\tmax_write_conn: %d\n"
           "\tmax_sync_conn: %d\n"
           "\tmax_core: %d\n"
           "\tis_daemon: %d\n"
           "\trole: %d\n"
           "\tmaster_sync_host: %s\n"
           "\tmaster_sync_port: %d\n"
           "\tsync_interval: %u\n"
           "\tuser: %s\n",
           conf.block_data_count_items, conf.dump_interval, conf.block_clean_cond,
           conf.block_clean_start, conf.block_clean_num, conf.read_port,
           conf.write_port, conf.sync_port, conf.datadir, conf.log_level,
           conf.log_name, conf.write_binlog, conf.timeout, conf.thread_num,
           conf.max_conn, conf.max_read_conn, conf.max_write_conn,
           conf.max_sync_conn, conf.max_core, conf.is_daemon, conf.role,
           conf.master_sync_host, conf.master_sync_port, conf.sync_interval,
           conf.user);

     return 1;
}

//set config info example
int
test_set_config_info()
{
    int ret;

    if ((ret = memlink_cmd_set_config_info(m, "timeout", "50")) != MEMLINK_OK) {
        DERROR("set config info error: %d\n", ret);
    }

    return 1;
}

int test_destroy()
{
	memlink_destroy(m);
	return 1;
}


int main(int argc, char **argv)
{
	test_init();
/*
    test_rm_key();
    test_create_key_list();
    int i;
    char buf[10];
    for (i = 0; i < 100; i++) {
        snprintf(buf, 100, "%d", i);
        test_insert_key_value(buf);
    }
*/
    test_tag(1);
    test_move();
    test_count();
    test_stat();
	test_destroy();

	return 1;
}

