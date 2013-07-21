<?php
include "memlinkclient.php";

date_default_timezone_set("Asia/Shanghai");

$valuesize = 20;

function timediff($starttm, $endtm)
{
	return ($endtm['sec'] - $starttm['sec']) * 1000000 + ($endtm['usec'] - $starttm['usec']);
}

function clearkey()
{
	$m = new MemLinkClient('127.0.0.1', 11001, 11002, 30);
	$key = "testmyhaha";
	
	$ret = $m->rmkey($key);
	if ($ret != MEMLINK_OK) {
		echo "rmkey error! $ret";
		return -1;
	}

	$m->destroy();

	return 0;
}

function test_insert($count)
{
	//echo "====== test_insert ======\n";
	$m = new MemLinkClient('127.0.0.1', 11001, 11002, 30);

	$key = "testmyhaha";
	$ret = $m->create($key, $valuesize, "4:3:1");
	if ($ret != MEMLINK_OK) {
		echo "create error $ret\n";
		return -1;
	}
	
	$maskstr = "8:3:1";
	$starttm = gettimeofday();
	
	for ($i = 0; $i < $count; $i++) {
		$val = sprintf("%0${valuesize}d", $i);
		//echo "insert $val\n";
		$ret = $m->insert($key, $val, strlen($val), $maskstr, 0);
		if ($ret != MEMLINK_OK) {
			echo "insert error $ret\n";
			return -2;
		}
	}

	$endtm = gettimeofday();
	$speed = $count / (timediff($starttm, $endtm) / 1000000);
	echo "use time: ".timediff($starttm, $endtm)." speed: $speed\n";

	$m->destroy();

	return $speed;
}

function test_insert_short($count)
{
	//echo "====== test_insert ======\n";
	$iscreate = 0;
	$key = "testmyhaha";
	$maskstr = "8:3:1";

	$starttm = gettimeofday();
	for ($i = 0; $i < $count; $i++) {
		$m = new MemLinkClient('127.0.0.1', 11001, 11002, 30);

		if ($iscreate == 0) {
			$ret = $m->create($key, $valuesize, "4:3:1");
			if ($ret != MEMLINK_OK) {
				echo "create error $ret\n";
				return -1;
			}
			$iscreate = 1;
		}


		$val = sprintf("%0${valuesize}d", $i);
		//echo "insert $val\n";
		$ret = $m->insert($key, $val, strlen($val), $maskstr, 0);
		if ($ret != MEMLINK_OK) {
			echo "insert error $ret\n";
			return -2;
		}
		$m->destroy();
	}

	$endtm = gettimeofday();
	$speed = $count / (timediff($starttm, $endtm) / 1000000);
	echo "use time: ".timediff($starttm, $endtm)." speed: $speed\n";

	return $speed;
}


function test_range($frompos, $dlen, $count)
{
	//echo "====== test_range ======\n";
	$key = "testmyhaha";
	$m = new MemLinkClient('127.0.0.1', 11001, 11002, 30);
	
	$starttm = gettimeofday();
	for ($i = 0; $i < $count; $i++) {
		$ret = $m->range($key, "", $frompos, $dlen);
		if (is_null($ret)) {
			echo "range error!\n";
			return -1;
		}
	}
	$endtm = gettimeofday();
	$speed = $count / (timediff($starttm, $endtm) / 1000000);
	echo "use time: ".timediff($starttm, $endtm)." speed: $speed\n";

	$m->destroy();

	return $speed;
}

function test_range_short($frompos, $dlen, $count)
{
	//echo "====== test_range ======\n";
	$key = "testmyhaha";
	$starttm = gettimeofday();

	for ($i = 0; $i < $count; $i++) {
		$m = new MemLinkClient('127.0.0.1', 11001, 11002, 30);

		$ret = $m->range($key, "", $frompos, $dlen);
		if (is_null($ret)) {
			echo "range error!\n";
			return -1;
		}
		$m->destroy();
	}
	$endtm = gettimeofday();
	$speed = $count / (timediff($starttm, $endtm) / 1000000);
	echo "use time: ".timediff($starttm, $endtm)." speed: $speed\n";

	return $speed;
}


function alltest()
{
	$testnum = array(10000, 100000, 1000000, 10000000);
	$insertret = array();
	$insertfunc = array(test_insert, test_insert_short);
	$insert_test_count = 4;

	$insert_testnum = array(10000, 100000);

	foreach ($insertfunc as $func) {
		foreach ($insert_testnum as $t) {
			$insertret = array();
			for ($i = 0; $i < $insert_test_count; $i++) {
				if (array_search($func, $insertfunc) == 0) {
					echo "====== insert long $t test: $i ======\n";
				}else{
					echo "====== insert short $t test: $i ======\n";
				}

				$speed = $func($t);
				clearkey();
				array_push($insertret, $speed);
			}

			sort($insertret);

			array_shift($insertret);
			array_pop($insertret);
		
			$sum = array_sum($insertret);
			$retlen = count($insertret);
			$sp = $sum / $retlen;
			echo "\33[31m====== sum:$sum count:$retlen speed: $sp ======\33[0m\n";
		}
	}

	$rangeret = array();
	$rangefunc = array(test_range, test_range_short);
	$rangetest = array(100, 200, 1000);
	$range_test_count = 8;

	foreach ($rangefunc as $func) {
		foreach ($testnum as $t) {
			test_insert($t);
			for ($j = 0; $j < 2; $j++) {
				foreach ($rangetest as $k) {
					$rangeret = array();
					for ($i = 0; $i < $range_test_count; $i++) {
						if ($j == 0) {
							$startpos = 0;
							$slen = $k;
						}else{
							$startpos = $t - $k;
							$slen = $k;
						}

						if (array_search($func, $rangefunc) == 0) {
							echo "====== range long $t from:$startpos len:$slen test:$i ======\n";
						}else{
							echo "====== range short $t from:$startpos len:$slen test:$i ======\n";
						}

						$speed = $func($startpos, $slen, 1000);
						array_push($rangeret, $speed);
					}

					sort($rangeret);
					array_shift($rangeret);
					array_pop($rangeret);
					
					$sum = array_sum($rangeret);
					$retlen = count($rangeret);
					$sp = $sum / $retlen;

					echo "\33[31m====== sum:$sum count:$retlen num:$t from:$startpos len:$slen speed: $sp ======\33[0m\n";

				}
			}
			clearkey();
		}
	}

}


if ($argc == 1) {
	alltest();
	exit(0);
}

if ($argc != 4) {
	echo "test.php count range_start range_len\n";
	exit(0);
}
$count = intval($argv[1]);
$range_start = intval($argv[2]);
$range_len   = intval($argv[3]);

//test_insert_short($count);
test_range($range_start, $range_len, 1000);

?>
