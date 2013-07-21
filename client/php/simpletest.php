<?php
include_once("memlinkclient.php");
/*
$class = new ReflectionClass("MemLink");
$methods = $class->getMethods();
foreach ($methods as $method) {
    echo "method:".$method->getName()."\n";
}
*/

$m = new MemLinkClient("127.0.0.1", 11001, 11002, 10);
$key = "haha";

//equal to create_list
$ret = $m->rmkey("akey");
if ($ret != MEMLINK_OK) {
    echo "rmkey \"akey\" error: $ret";
    exit(-1);
}

$ret = $m->rmkey("bkey");
if ($ret != MEMLINK_OK) {
    echo "rmkey \"bkey\" error: $ret";
    exit(-1);
}
$ret = $m->rmkey("ckey");
if ($ret != MEMLINK_OK) {
    echo "rmkey \"ckey\" error: $ret";
    exit(-1);
}
$ret = $m->rmkey("dkey");
if ($ret != MEMLINK_OK) {
    echo "rmkey \"dkey\" error: $ret";
    exit(-1);
}

$ret = $m->rmkey($key);
if ($ret != MEMLINK_OK) {
    echo "rmkey \"$key\" error: $ret";
    exit(-1);
}

$ret = $m->create("akey", 10, "1:1:1", MEMLINK_LIST, MEMLINK_VALUE_OBJ);
if ($ret != MEMLINK_OK) {
    echo "create error: $ret\n";
    exit(-1);
}

//equal to create_queue
$ret = $m->create("bkey", 10, "1:1:1", MEMLINK_QUEUE, MEMLINK_VALUE_OBJ);
if ($ret != MEMLINK_OK) {
    echo "create error: $ret\n";
    exit(-1);
}

$ret = $m->create_queue("ckey", 10, "1:1:1");
if ($ret != MEMLINK_OK) {
    echo "create queue error: $ret\n";
    exit(-1);
}

$ret = $m->create_sortlist("dkey", 10, "1:1:1", MEMLINK_VALUE_OBJ);
if ($ret != MEMLINK_OK) {
    echo "create sortlist error: $ret\n";
    exit(-1);
}

$ret = $m->ping();
if ($ret != MEMLINK_OK) {
    echo "ping error: $ret\n";
    exit(-1);
}

$ret = $m->dump();
if ($ret != MEMLINK_OK) {
    echo "dump error: $ret\n";
    exit(-1);
}

$ret = $m->create_list($key, 12, "4:3:1");
if ($ret != MEMLINK_OK) {
	echo "create error: $key\n";
	exit(-1);
}

for ($i = 0; $i < 100; $i++) {
	$val = sprintf("%012d", $i);
	$ret = $m->insert($key, $val, strlen($val), "8:1:1", 0);
	if ($ret != MEMLINK_OK) {
		echo "insert error! $ret $key $val\n";
		exit(-1);
	}
}

$ret = $m->stat($key);
if (is_null($ret)) {
	echo "stat error: $key\n";
	exit(-1);
}
echo "stat, data_used: $ret->data_used\n";

$stat = new MemLinkStat();
$ret = $m->stat2($key, $stat);
if ($ret != MEMLINK_OK) {
    echo "stat2 error: $ret\n";
    exit(-1);
}
echo "visible: $stat->visible tagdel: $stat->tagdel\n";


$stat = new MemLinkStat();
$ret = $m->stat2($key, $stat);
if ($ret != MEMLINK_OK) {
    echo "stat2 error: $ret\n";
    exit(-1);
}
echo "visible: $stat->visible tagdel: $stat->tagdel\n";

$ret = $m->range_visible($key, "::", 0, 10);
if (is_null($ret)) {
	echo "range error!";
	exit(-1);
}

echo "range, count: $ret->count\n";

$item = $ret->root;
while ($item) {
	echo "value: $item->value\n";
	$item = $item->next;
}

for ($i = 0; $i < 10; $i++) {
	$val = sprintf("%012d", $i);
	$ret = $m->delete($key, $val, strlen($val));
	if ($ret != MEMLINK_OK) {
		echo "delete error! $ret \n";
		exit(0);
	}
}

$count =$m->count($key, '');
if ($count) {
    echo "count: $count->visible_count $count->tagdel_count \n";
}

$count = new MemLinkCount();
$ret = $m->count2($key, '', $count);
if ($ret != MEMLINK_OK) {
    echo "count2 error: $ret\n";
    exit(-1);
}
echo "count: $count->visible_count $count->tagdel_count \n";

/*
$info = new MemLinkStatSys();
$ret = $m->stat_sys2($info);
if ($ret == MEMLINK_OK) {
    echo "keys:$info->keys\nvalues:$info->values\n";
}else{
    echo "stat sys error! $ret\n";
}*/

$info = $m->stat_sys();
if ($info != NULL) {
    echo "keys:$info->keys\nvalues:$info->values\n";
}

$ret = $m->clean($key);
if ($ret != MEMLINK_OK) {
    echo "clean error: $ret\n";
    exit(-1);
}

$ret = $m->move($key, "000000000096", 12, 1);
if ($ret != MEMLINK_OK) {
    echo "move error: $ret\n";
    exit(-1);
}

$ret = $m->tag($key, "000000000096", 12, 1);
if ($ret != MEMLINK_OK) {
    echo "tag error: $ret\n";
    exit(-1);
}

$ret = $m->range_visible($key, "::", 0, 10);
if (is_null($ret)) {
	echo "range error!";
	exit(-1);
}
$item = $ret->root;
while ($item) {
	echo "value: $item->value\n";
	$item = $item->next;
}

$result = new MemLinkResult();
$ret = $m->range2_visible($key, "::", 0, 10, $result);;
if ($ret != MEMLINK_OK) {
    echo "range2_visible error: $ret\n";
    exit(-1);
}
$item = $result->root;
while ($item) {
	echo "value: $item->value\n";
	$item = $item->next;
}

$ret = $m->lpush($key, "aa", 2, "1:2:1");
if ($ret != MEMLINK_OK) {
    echo "lpush error: $ret\n";
    exit(-1);
}

$ret = $m->rpush($key, "bb", 2, "1:2:1");
if ($ret != MEMLINK_OK) {
    echo "rpush error: $ret\n";
    exit(-1);
}

$result = new MemLinkResult();
$ret = $m->range2_visible($key, "::", 0, 100, $result);;
if ($ret != MEMLINK_OK) {
    echo "range2_visible error: $ret\n";
    exit(-1);
}
$item = $result->root;
while ($item) {
	echo "value: $item->value\n";
	$item = $item->next;
}

$ret = $m->lpop($key, 1);
if ($ret == NULL) {
    echo "lpop error: $ret\n";
    exit(-1);
}

echo "lpopd:\n";
$item = $ret->root;
while ($item) {
	echo "value: $item->value\n";
	$item = $item->next;
}

$result = new MemLinkResult();
$ret = $m->lpop2($key, 1, $result);
if ($ret != MEMLINK_OK) {
    echo "lpop2 error: $ret\n";
    exit(-1);
}

$item = $result->root;
while ($item) {
	echo "value: $item->value\n";
	$item = $item->next;
}

$ret = $m->rpop($key, 1);
if ($ret == NULL) {
    echo "rpop error: $ret\n";
    exit(-1);
}

echo "rpopd:\n";
$item = $ret->root;
while ($item) {
	echo "value: $item->value\n";
	$item = $item->next;
}

$result = new MemLinkResult();
$ret = $m->rpop2($key, 1, $result);
if ($ret != MEMLINK_OK) {
    echo "rpop2 error: $ret\n";
    exit(-1);
}

$item = $result->root;
while ($item) {
	echo "value: $item->value\n";
	$item = $item->next;
}

for ($i = 0; $i < 10; $i++) {
	$val = sprintf("a%d", $i);
        $ret = $m->sortlist_insert("dkey", $val, strlen($val), "1:1:1");
	if ($ret != MEMLINK_OK) {
		echo "sortlist insert error! $ret\n";
		exit(-1);
	}
}

$ret = $m->sortlist_range("dkey", MEMLINK_VALUE_VISIBLE, "a2", 2, "a9", 2, "");
if ($ret == NULL) {
    echo "sortlist range error: $ret\n";
    exit(-1);
}
echo "sortlist:\n";
$item = $ret->root;
while ($item) {
	echo "value: $item->value\n";
	$item = $item->next;
}

$rcinfo = $m->read_conn_info();
if ($rcinfo == NULL) {
    echo "read_conn_info error: $ret\n";
    exit(-1);
}

echo "read conncount: $rcinfo->conncount\n";
$item = $rcinfo->root;
while ($item) {
    echo "client_ip: $item->client_ip\n";
    $item = $item->next;
}

$m->rcinfo_free($rcinfo);

$wcinfo = $m->write_conn_info();
if ($rcinfo == NULL) {
    echo "write_conn_info error: $ret\n";
    exit(-1);
}

echo "write conncount: $wcinfo->conncount\n";
$item = $wcinfo->root;
while ($item) {
    echo "client_ip: $item->client_ip\n";
    $item = $item->next;
}

$m->wcinfo_free($wcinfo);

$scinfo = $m->sync_conn_info();
if ($scinfo == NULL) {
    echo "sync_conn_info error: $ret\n";
    exit(-1);
}

echo "sync conncount: $scinfo->conncount\n";
$item = $scinfo->root;
while ($item) {
    echo "client_ip: $item->client_ip\n";
    $item = $item->next;
}

$m->scinfo_free($scinfo);

$keyvalues = array(array('key'=>'dkey', 'value'=>'d1', 'mask'=>'1:1:1', 'pos'=>0),
                   array('key'=>'dkey', 'value'=>'d2', 'mask'=>'1:1:1', 'pos'=>0),
                   array('key'=>'dkey', 'value'=>'b1', 'mask'=>'1:1:1', 'pos'=>0),
                   array('key'=>'dkey', 'value'=>'b2', 'mask'=>'1:1:1', 'pos'=>0));
$ret = $m->insert_mkv($keyvalues);
$m->mkv_destroy($ret);

$ret = $m->sortlist_range("dkey", MEMLINK_VALUE_VISIBLE, "a0", 2, "z9", 2, "");
if ($ret == NULL) {
    echo "sortlist range error: $ret\n";
    exit(-1);
}
echo "sortlist:\n";
$item = $ret->root;
while ($item) {
	echo "value: $item->value\n";
	$item = $item->next;
}

$m->destroy();
?>
