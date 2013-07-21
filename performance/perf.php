<?php
include "memlinkclient.php";

date_default_timezone_set("Asia/Shanghai");

function timediff($starttm, $endtm)
{
	return ($endtm['sec'] - $starttm['sec']) * 1000000 + ($endtm['usec'] - $starttm['usec']);
}

class MemLinkPerf
{
	private $host  = '127.0.0.1';
	private $rport = 11011;
	private $wport = 11012;
	private $timeout = 30;

	function __construct() 
	{
		
	}

	function __destruct() 
	{
	}

	function dotest($args) 
	{
		$n = 1;
		$doing = 'create';

		$funcname = "test_" . $doing;
		$start = gettimeofday();
		$this->$funcname($args);
		$end   = gettimeofday();
		
		$tm = timediff($start, $end);
		$speed = $n / $tm * 1000000;
		echo "use time: $tm, speed:$speed\n";
	}

	function test_create($args)
	{
		return $this->test_create_list($args);
	}

	function _test_create($args, $listtype)
	{
		if ($args['c'] == 1) {
		}else{
		}
	}

	function test_create_list($args)
	{
		return $this->_test_create($args, MEMLINK_LIST);
	}

	function test_create_queue($args)
	{
		return $this->_test_create($args, MEMLINK_QUEUE);
	}

	function test_create_sortlist($args)
	{
		return $this->_test_create($args, MEMLINK_SORTLIST);
	}
	
	function test_insert($args)
	{
	}

	function test_range($args)
	{
	}
	
	function test_move($args)
	{
	}

	function test_del($args)
	{
	}

	function test_mask($args)
	{
	}

	function test_tag($args)
	{
	}

	function test_count($args)
	{
	}
	

};

function show_help()
{
	printf("usage:\n\tperf [options]\n");
    printf("options:\n");
    printf("\t--help,-h\tshow help information\n");
    printf("\t--thread,-t\tthread count for test\n");
    printf("\t--count,-n\tinsert number\n");
    printf("\t--from,-f\trange from position\n");
    printf("\t--len,-l\trange length\n");
    printf("\t--do,-d\t\tcreate/insert/range/move/del/mask/tag/count\n");
    printf("\t--key,-k\tkey\n");
    printf("\t--value,-v\tvalue\n");
    printf("\t--valuesize,-s\tvaluesize for create\n");
    printf("\t--pos,-p\tposition for insert\n");
    printf("\t--popnum,-o\tpop number\n");
    printf("\t--kind,-i\tkind for range. all/visible/tagdel\n");
    printf("\t--mask,-m\tmask str\n");
    printf("\t--longconn,-c\tuse long connection for test. default 1\n");
    printf("\n\n");

	exit(0);
}

function main()
{
    $optstr = "ht:n:f:l:k:v:s:p:o:i:a:m:d:c:";
	$opts = getopt($optstr);

    $test = new MemLinkPerf();
    $test->dotest($opts);
}

main();

?>
