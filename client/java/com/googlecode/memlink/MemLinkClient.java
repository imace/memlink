package com.googlecode.memlink;

import java.util.*;


public class MemLinkClient
{
	private MemLink client = null;
	
	static {
		try{
			System.loadLibrary("cmemlink");
		}catch (UnsatisfiedLinkError e) {
			System.err.println("library libcmemlink.so load error! " + e);
			System.exit(1);
		}
	}

	public MemLinkClient (String host, int readport, int writeport, int timeout)
	{
		//client = new MemLink(host, readport, writeport, timeout);
		client = cmemlink.memlink_create(host, readport, writeport, timeout);
	}

	public void finalize()
	{
		destroy();
	}

	public void close()
	{
		if (client != null) {
			cmemlink.memlink_close(client);
		}
	}

	public void destroy()
	{
		if (client != null) {
			cmemlink.memlink_destroy(client);
			client = null;
		}
	}

	public int createTable(String table, int valuesize, String attrstr, short listtype, short valuetype)
	{
		return cmemlink.memlink_cmd_create_table(client, table, valuesize, attrstr, listtype, valuetype);
	}
    
    public int createTableList(String table, int valuesize, String attrstr)
	{
		return cmemlink.memlink_cmd_create_table_list(client, table, valuesize, attrstr);
	}

    public int createTableQueue(String table, int valuesize, String attrstr)
	{
		return cmemlink.memlink_cmd_create_table_queue(client, table, valuesize, attrstr);
	}

    public int createTableSortList(String table, int valuesize, String attrstr, int valuetype)
	{
		return cmemlink.memlink_cmd_create_table_sortlist(client, table, valuesize, attrstr, (short)valuetype);
	}

	public int removeTable(String table) 
	{	
		return cmemlink.memlink_cmd_remove_table(client, table);
	}

	public int createNode(String table, String key) 
	{
		return cmemlink.memlink_cmd_create_node(client, table, key);
	}

	public int dump()
	{
		return cmemlink.memlink_cmd_dump(client);
	}

	public int clean(String table, String key)
	{
		return cmemlink.memlink_cmd_clean(client, table, key);
	}

	public int stat(String table, String key, MemLinkStat ms)
	{
		return cmemlink.memlink_cmd_stat(client, table, key, ms);
	}

	public int delete(String table, String key, byte []value)
	{
		return cmemlink.memlink_cmd_del(client, table, key, value);
	}

	public int insert(String table, String key, byte []value, String attrstr, int pos)
	{
		return cmemlink.memlink_cmd_insert(client, table, key, value, attrstr, pos);
	}

	public int move(String table, String key, byte []value, int pos)
	{
		return cmemlink.memlink_cmd_move(client, table, key, value, pos);
	}

	public int attr(String table, String key, byte []value, String attrstr)
	{
		return cmemlink.memlink_cmd_attr(client, table, key, value, attrstr);
	}

	public int tag(String table, String key, byte []value, int tag)
	{
		return cmemlink.memlink_cmd_tag(client, table, key, value, tag);
	}

	public int range(String table, String key, int kind, String attrstr, int frompos, int len, MemLinkResult result)
	{
		/*MemLinkResult result = new MemLinkResult();
		int ret = cmemlink.memlink_cmd_range(client, key, kind, attrstr, frompos, len, result);
		if (ret == cmemlink.MEMLINK_OK) {
			return result;
		}
		return null;*/

		return cmemlink.memlink_cmd_range(client, table, key, kind, attrstr, frompos, len, result);
	}


	public int rangeVisible(String table, String key, String attrstr, int frompos, int len, MemLinkResult result)
    {
        return range(table, key, cmemlink.MEMLINK_VALUE_VISIBLE, attrstr, frompos, len, result);
    }
	
    public int rangeTagdel(String table, String key, String attrstr, int frompos, int len, MemLinkResult result)
    {
        return range(table, key, cmemlink.MEMLINK_VALUE_TAGDEL, attrstr, frompos, len, result);
    }

	public int rangeAll(String table, String key, String attrstr, int frompos, int len, MemLinkResult result)
    {
        return range(table, key, cmemlink.MEMLINK_VALUE_ALL, attrstr, frompos, len, result);
    }

	public int rmkey(String table, String key)
	{
		return cmemlink.memlink_cmd_rmkey(client, table, key);
	}

	public int count(String table, String key, String attrstr, MemLinkCount count) 
	{
		return cmemlink.memlink_cmd_count(client, table, key, attrstr, count);
	}	

    public int lpush(String table, String key, byte []value, String attrstr) 
    {
        return cmemlink.memlink_cmd_lpush(client, table, key, value, attrstr);
    }

    public int rpush(String table, String key, byte []value, String attrstr) 
    {
        return cmemlink.memlink_cmd_rpush(client, table, key, value, attrstr);
    }

    public int lpop(String table, String key, int num, MemLinkResult result)
    {
        return cmemlink.memlink_cmd_lpop(client, table, key, num, result);
    }

    public int rpop(String table, String key, int num, MemLinkResult result)
    {
        return cmemlink.memlink_cmd_rpop(client, table, key, num, result);
    }
    
    public int sortListInsert(String table, String key, byte []value, String attrstr)
    {
        return cmemlink.memlink_cmd_sortlist_insert(client, table, key, value, attrstr);
    }

    public int sortListRange(String table, String key, int kind, byte []vmin, byte []vmax, 
							String attrstr, MemLinkResult result)
    {
        return cmemlink.memlink_cmd_sortlist_range(client, table, key, kind, attrstr, vmin, vmax, result);
    }

    public int sortListRangeAll(String table, String key, byte []vmin, byte []vmax, String attrstr, MemLinkResult result)
    {
        return sortListRange(table, key, cmemlink.MEMLINK_VALUE_ALL, vmin, vmax, attrstr, result);
    }

    public int sortListRangeVisible(String table, String key, byte []vmin, byte []vmax, String attrstr, MemLinkResult result)
    {
        return sortListRange(table, key, cmemlink.MEMLINK_VALUE_VISIBLE, vmin, vmax, attrstr, result);
    }

    public int sortListRangeTagdel(String table, String key, byte []vmin, byte []vmax, String attrstr, MemLinkResult result)
    {
        return sortListRange(table, key, cmemlink.MEMLINK_VALUE_TAGDEL, vmin, vmax, attrstr, result);
    }


    public int sortListDelete(String table, String key, int kind, byte []vmin, byte []vmax, String attrstr)
    {
        return cmemlink.memlink_cmd_sortlist_del(client, table, key, kind, attrstr, vmin, vmax);
    }

    public int sortListCount(String table, String key, byte []vmin, byte []vmax, String attrstr, MemLinkCount count)
    {
        return cmemlink.memlink_cmd_sortlist_count(client, table, key, attrstr, vmin, vmax, count);
    }

    public int readConnInfo(MemLinkRcInfo info)
    {
        return cmemlink.memlink_cmd_read_conn_info(client, info);
    }

    public int writeConnInfo(MemLinkWcInfo info)
    {
        return cmemlink.memlink_cmd_write_conn_info(client, info);
    }

    public int syncConnInfo(MemLinkScInfo info)
    {
        return cmemlink.memlink_cmd_sync_conn_info(client, info);
    }

    public int insertMKV(MemLinkInsertMkv mkv)
    {
        return cmemlink.memlink_cmd_insert_mkv(client, mkv);
    }
}
