<?php
include_once("cmemlink.php");

class MemLinkClient
{
    private $client;

    function __construct($host, $readport, $writeport, $timeout)
    {
        $this->client = memlink_create($host, $readport, $writeport, $timeout);
        //$r = memlink_create($host,$readport,$writeport,$timeout);
        //$this->client = is_resource($r) ? new MemLink($r) : $r;
    }

    function close()
    {
        if ($this->client) {
            memlink_close($this->client);
        }
    }

    function destroy()
    {
        if ($this->client) {
            memlink_destroy($this->client);
        }
        $this->client = null;
    }

	function create_table($table, $valuesize, $attrformat, $listtype, $valuetype)
	{
    	if( false == is_int($valuesize) or false == is_string($table) or false == is_string($attrformat) )
    	{
    		return -1;
    	}
    	
		return memlink_cmd_create($this->client, $table, $valuesize, $attrformat, $listtype, $valuetype);
	}

	function create_table_list($table, $valuesize, $attrformat)
	{
    	if( false == is_int($valuesize) or false == is_string($table) or false == is_string($attrformat) )
    	{
    		return -1;
    	}
    	
		return memlink_cmd_create_list($this->client, $table, $valuesize, $attrformat);
	}

	function create_table_queue($table, $valuesize, $attrformat)
	{
    	if( false == is_int($valuesize) or false == is_string($table) or false == is_string($attrformat) )
    	{
    		return -1;
    	}
    	
		return memlink_cmd_create_queue($this->client, $table, $valuesize, $attrformat);
	}

    function create_table_sortlist($table, $valuesize, $attrformat, $valuetype)
	{
    	if( false == is_int($valuesize) or false == is_string($table) or false == is_string($attrformat) )
    	{
    		return -1;
    	}
    	
		return memlink_cmd_create_sortlist($this->client, $table, $valuesize, $attrformat, $valuetype);
	}
	
	function remove_table($table)
	{
		if (false == is_string($table)) {
			return -1;
		}
		return memlink_cmd_remove_table($this->client, $table);
	}

	
	function create_node($table, $key)
	{
		if (false == is_string($table) or false == is_string($key)) {
			return -1;
		}
		return memlink_cmd_create_node($this->client, $table, $key);
	}

	function ping()
    {
        return memlink_cmd_ping($this->client);
    }

    function dump()
    {
        return memlink_cmd_dump($this->client);
    }

    function clean($table, $key)
    {
    	if(false == is_string($table) || false == is_string($key)) {
    		return -1;
    	}
        return memlink_cmd_clean($this->client, $table, $key);
    }

    function stat($table, $key)
    {
    	if( false == is_string($table) || false == is_string($key))
    	{
    		return NULL;
    	}
    
        $mstat = new MemLinkStat();

        $ret = memlink_cmd_stat($this->client, $table, $key, $mstat);
		if ($ret == MEMLINK_OK) {
			return $mstat;
		}
		return NULL;
    }

	function stat2($table, $key, $mstat)
	{
    	if( false == is_string($table) || false == is_string($key)) {
    		return -1;
    	}
	
        return memlink_cmd_stat($this->client, $table, $key, $mstat);
	}

    function stat_sys()
    {
        $stat = new MemLinkStatSys();
        $ret = memlink_cmd_stat_sys($this->client, $stat);
        if ($ret == MEMLINK_OK) {
            return $stat;
        }
        return NULL;
    }

    function stat_sys2($stat)
    {
        return memlink_cmd_stat_sys($this->client, $stat);
    }

    function delete($table, $key, $value, $valuelen)
    {
    	if( false == is_string($table) || false == is_string($key) or false == is_int($valuelen) )
    	{
    		return -1;
    	}
    	
        return memlink_cmd_del($this->client, $table, $key, $value, $valuelen);
    }

    function insert($table, $key, $value, $valuelen, $attrstr, $pos)
    {
    	if( false == is_string($table) || false == is_string($key) or false == is_int($valuelen) or
    		false == is_int($pos) or false == is_string($attrstr) )
    	{
    		return -1;
    	}
		
        return memlink_cmd_insert($this->client, $table, $key, $value, $valuelen, $attrstr, $pos);
    }

    function move($table, $key, $value, $valuelen, $pos)
    {
    	if( false == is_string($table) || false == is_string($key) or false == is_int($valuelen) or false == is_int($pos) )
    	{
    		return -1;
    	}
			
        return memlink_cmd_move($this->client, $table, $key, $value, $valuelen, $pos);
    }

    function attr($table, $key, $value, $valuelen, $attrstr)
    {
    	if( false == is_string($table) || false == is_string($key) or false == is_int($valuelen) or false == is_string($attrstr) )
    	{
    		return -1;
    	}
    
        return memlink_cmd_attr($this->client, $table, $key, $value, $valuelen, $attrstr);
    }

    function tag($table, $key, $value, $valuelen, $tag)
    {
    	if( false == is_string($table) || false == is_string($key) or false == is_int($valuelen) or false == is_int($tag) )
    	{
    		return -1;
    	}
    	
        return memlink_cmd_tag($this->client, $table, $key, $value, $valuelen, $tag);
    }

    function range($table, $key, $kind, $attrstr, $frompos, $len)
    {
    	if( false == is_string($table) || false == is_string($key) or false == is_int($frompos) or
    		false == is_int($len) or false == is_string($attrstr) )
    	{
    		return NULL;
    	}
    	
        $result = new MemLinkResult();
        
        $ret = memlink_cmd_range($this->client, $table, $key, $kind, $attrstr, $frompos, $len, $result);
		if ($ret == MEMLINK_OK) {
			return $result;
		}
		return NULL;
    }

	function range_visible($table, $key, $attrstr, $frompos, $len) 
	{
		return $this->range($table, $key, MEMLINK_VALUE_VISIBLE, $attrstr, $frompos, $len);
	}
	
	function range_tagdel($table, $key, $attrstr, $frompos, $len) 
	{
		return $this->range($table, $key, MEMLINK_VALUE_TAGDEL, $attrstr, $frompos, $len);
	}

	function range_all($table, $key, $attrstr, $frompos, $len) 
	{
		return $this->range($table, $key, MEMLINK_VALUE_ALL, $attrstr, $frompos, $len);
	}

	function range2($table, $key, $kind, $attrstr, $frompos, $len, $result)
	{
    	if( false == is_string($table) || false == is_string($key) or false == is_int($frompos) or false == is_int($len) or false == is_string($attrstr) )
    	{
    		return -1;
    	}
	
		return memlink_cmd_range($this->client, $table, $key, $kind, $attrstr, $frompos, $len, $result);
	}

	function range2_visible($table, $key, $attrstr, $frompos, $len, $result)
	{
		return $this->range2($table, $key, MEMLINK_VALUIE_VISIBLE, $attrstr, $frompos, $len, $result);
	}

	function range2_tagdel($table, $key, $attrstr, $frompos, $len, $result)
	{
		return $this->range2($table, $key, MEMLINK_VALUIE_TAGDEL, $attrstr, $frompos, $len, $result);
	}

	function range2_all($table, $key, $attrstr, $frompos, $len, $result)
	{
		return $this->range2($table, $key, MEMLINK_VALUIE_ALL, $attrstr, $frompos, $len, $result);
	}

	function rmkey($table, $key)
	{
    	if( false == is_string($table) || false == is_string($key) ) {
    		return -1;
    	}
	
		return memlink_cmd_rmkey($this->client, $table, $key);
	}

	function count($table, $key, $attrstr)
	{
    	if( false == $table || false == is_string($key) or false == is_string($attrstr) ) {
    		return NULL;
    	}
	
		$count = new MemLinkCount();
		$ret = memlink_cmd_count($this->client, $table, $key, $attrstr, $count);
		if ($ret == MEMLINK_OK) {
			return $count;
		}
		return NULL;
	}

	function count2($table, $key, $attrstr, $count)
	{
    	if( false == is_string($table) || false == is_string($key) or false == is_string($attrstr) ) {
    		return -1;
    	}
	
		return memlink_cmd_count($this->client, $table, $key, $attrstr, $count);
	}

    function lpush($table, $key, $value, $valuelen, $attrstr="")
    {
    	if( false == is_string($table) || false == is_string($key) or false == is_string($attrstr) ) {
    		return -1;
    	}
	
        return memlink_cmd_lpush($this->client, $table, $key, $value, $valuelen, $attrstr);
    }

    function rpush($table, $key, $value, $valuelen, $attrstr="")
    {
    	if( false == is_string($table) || false == is_string($key) or false == is_string($attrstr) ) {
    		return -1;
    	}
	
        return memlink_cmd_rpush($this->client, $table, $key, $value, $valuelen, $attrstr);
    }

    function lpop($table, $key, $num=1)
    {
        $result = new MemLinkResult();
        $ret = memlink_cmd_lpop($this->client, $table, $key, $num, $result);
		if ($ret == MEMLINK_OK) {
			return $result;
		}
		return NULL;
    }

    function lpop2($table, $key, $num, $result)
    {
        return memlink_cmd_lpop($this->client, $table, $key, $num, $result);
    }


    function rpop($table, $key, $num=1)
    {
        $result = new MemLinkResult();
        $ret = memlink_cmd_lpop($this->client, $table, $key, $num, $result);
		if ($ret == MEMLINK_OK) {
			return $result;
		}
		return NULL;
    }
    
    function rpop2($table, $key, $num, $result)
    {
        return memlink_cmd_rpop($this->client, $table, $key, $num, $result);
    }
    
    function sortlist_insert($table, $key, $value, $valuelen, $attrstr="")
    {
        return memlink_cmd_sortlist_insert($this->client, $table, $key, $value, $valuelen, $attrstr);
    }

    function sortlist_range($table, $key, $kind, $vmin, $vminlen, $vmax, $vmaxlen, $attrstr='')
    {
        $result = new MemLinkResult();
        $ret = memlink_cmd_sortlist_range($this->client, $table, $key, $kind, $attrstr, $vmin, $vminlen, $vmax, $vmaxlen, $result);
		if ($ret == MEMLINK_OK) {
			return $result;
		}
		return NULL;
    }

    function sortlist_range2($table, $key, $kind, $vmin, $vminlen, $vmax, $vmaxlen, $attrstr, $result)
    {
        return memlink_cmd_sortlist_range($this->client, $table, $key, $kind, $attrstr, $vmin, $vminlen, $vmax, $vmaxlen, $result);
    }

    function sortlist_del($table, $key, $kind, $vmin, $vminlen, $vmax, $vmaxlen, $attrstr='')
    {
        return memlink_cmd_sortlist_del($this->client, $table, $key, $kind, $attrstr, $vmin, $vminlen, $vmax, $vmaxlen);
    }

    function sortlist_count($table, $key, $vmin, $vminlen, $vmax, $vmaxlen, $attrstr='')
    {
        $result = new MemLinkCount();
        $ret = memlink_cmd_sortlist_count($this->client, $table, $key, $attrstr, $vmin, $vminlen, $vmax, $vmaxlen, $result);
        if ($ret == MEMLINK_OK) {
            return $result;
        }
        return NULL;
    }

    function sortlist_count2($table, $key, $vmin, $vminlen, $vmax, $vmaxlen, $attrstr, $result)
    {
        return memlink_cmd_sortlist_count($this->client, $table, $key, $attrstr, $vmin, $vminlen, $vmax, $vmaxlen, $result);
    }

    function read_conn_info()
    {
        $rcinfo = new MemLinkRcInfo();
        $ret = memlink_cmd_read_conn_info($this->client, $rcinfo);
        if ($ret == MEMLINK_OK) {
            return $rcinfo;
        }
        return NULL;
    }
    function write_conn_info()
    {
        $wcinfo = new MemLinkWcInfo();
        $ret = memlink_cmd_write_conn_info($this->client, $wcinfo);
        if ($ret == MEMLINK_OK) {
            return $wcinfo;
        }
        return NULL;
    }
    function sync_conn_info()
    {
        $scinfo = new MemLinkScInfo();
        $ret = memlink_cmd_sync_conn_info($this->client, $scinfo);
        if ($ret == MEMLINK_OK) {
            return $scinfo;
        }
        return NULL;
    }
    static function memlink_imkv_create() {
        $r=memlink_imkv_create();
        if (is_resource($r)) {
            $c=substr(get_resource_type($r), (strpos(get_resource_type($r), '__') ? strpos(get_resource_type($r), '__') + 2 : 3));
            if (!class_exists($c)) {
                return new MemLinkInsertMkv($r);
            }
            return new $c($r);
        }
        return $r;
    }
    static function memlink_ikey_create($table, $key,$keylen) {
        $r=memlink_ikey_create($table, $key,$keylen);
        if (is_resource($r)) {
            $c=substr(get_resource_type($r), (strpos(get_resource_type($r), '__') ? strpos(get_resource_type($r), '__') + 2 : 3));
            if (!class_exists($c)) {
                return new MemLinkInsertKey($r);
            }
            return new $c($r);
        }
        return $r;
    }
    static function memlink_ival_create($value,$valuelen,$attrstr,$pos) {
        $r=memlink_ival_create($value,$valuelen,$attrstr,$pos);
        if (is_resource($r)) {
            $c=substr(get_resource_type($r), (strpos(get_resource_type($r), '__') ? strpos(get_resource_type($r), '__') + 2 : 3));
            if (!class_exists($c)) {
                return new MemLinkInsertVal($r);
            }
            return new $c($r);
        }
        return $r;
    }

    function insert_mkv($array)
    {
        $mkv = $this->memlink_imkv_create();
        foreach ($array as $item) {
            if (is_array($item) && count($item) == 4) {
                if (is_null($item["key"]) || is_null($item["value"]) || 
                    is_null($item["attr"]) || is_null($item["pos"])) {
                    return $mkv;
                }
                $keyobj = $this->memlink_ikey_create($item["key"], strlen($item["key"]));
                $valobj = $this->memlink_ival_create($item["value"], strlen($item["value"]), $item["attr"], $item["pos"]);
                $ret = memlink_ikey_add_value($keyobj, $valobj); 
                if ($ret != MEMLINK_OK) {
                    return $mkv;
                }
                $ret = memlink_imkv_add_key($mkv, $keyobj);
                if ($ret != MEMLINK_OK) {
                    return $mkv;
                }
            }
        }
        $ret = memlink_cmd_insert_mkv($this->client, $mkv);
        return $mkv;
    }

    function rcinfo_free($info) {
        return memlink_rcinfo_free($info);
    }

    function wcinfo_free($info) {
        return memlink_wcinfo_free($info);
    }

    function scinfo_free($info) {
        return memlink_scinfo_free($info);
    }

    function mkv_destroy($mkv) {
        return memlink_imkv_destroy($mkv);
    }
}

?>
