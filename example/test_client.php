<?
include_once("memlinkclient.php");

function test_create($m, $key)
{
    $maskformat = "4:3:1";
    $valuesize  = 12;
    $ret = $m->create_list($key, $valuesize, $maskformat);
    if ($ret != MEMLINK_OK) {
        echo "create error: $ret\n";
        return -1;
    }
    return 0;
}

function test_insert($m, $key)
{
    for ($i = 0; $i < 100; $i++) {
        $val = sprintf("%012d", $i);
        $ret = $m->insert($key, $val, strlen($val), "8:1:1", $i);
        if ($ret != MEMLINK_OK) {
            echo "insert error! $ret\n";
            return -1;
        }
    }  
    return 0;
}

function test_stat($m, $key)
{
    $ret = $m->stat($key);
    if (is_null($ret)) {
        echo "stat error!\n";
        return -1;
    }

    echo "stat blocks: $ret->blocks\n";
    return 0;
}

function test_delete($m, $key)
{
    $value = sprintf("%012d", 1);
    $valuelen = strlen($value);
	
	echo "delete $key => $value \n";
    $ret = $m->delete($key, $value, $valuelen);
    if ($ret != MEMLINK_OK) {
        echo "delete error: $ret\n";
        return -1;
    }
    return 0;
}

function test_tag($m, $key)
{
    $value = sprintf("%012d", 2);
    $valuelen = strlen($value);
	echo "tag delete $key, $value\n";
	$ret = $m->tag($key, $value, $valuelen, MEMLINK_TAG_DEL);
	if ($ret != MEMLINK_OK) {
		echo "tag error: $ret \n";
		return -1;
	}
	return 0;
}


function test_range($m, $key)
{
    $ret = $m->range($key, MEMLINK_VALUE_VISIBLE, "", 0, 10);
    if (is_null($ret)) {
        echo "range error! \n";
        return -1;
    }

    echo "range count: $ret->count \n";

    $item = $ret->root;

    while ($item) {
        echo "range item: $item->value  $item->mask \n";
        $item = $item->next;
    }
    return 0;
}

function test_count($m, $key)
{
    $ret = $m->count($key, "");
    if (is_null($ret)) {
        echo "range error! \n";
        return -1;
    }

    return 0;
}

$m = new MemLinkClient("127.0.0.1", 11001, 11002, 10);
$key = "haha";

test_create($m, $key);
test_stat($m, $key);
test_insert($m, $key);
test_count($m, $key);
test_range($m, $key);
test_tag($m, $key);
test_delete($m, $key);
test_range($m, $key);

?>
