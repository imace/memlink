import com.googlecode.memlink.*;
import java.util.*;
import java.lang.*;
import java.nio.*;

public class Test
{
	static {
        try{
			System.out.println("try load library: libcmemlink.so");
            System.loadLibrary("cmemlink");
        }catch (UnsatisfiedLinkError e) {
            System.err.println("library libcmemlink.so load error! " + e); 
            System.exit(1);
        }   
		System.out.println("load ok!");
    }
    
	public static void testQueue()
    {
		MemLinkClient m = new MemLinkClient("127.0.0.1", 11001, 11002, 10);
        int i;
		String name = "table1";
        String key = name + ".haha";
        
        m.rmkey(key);

        int ret;

        ret = m.createTableQueue(name, 10, "4:3:1");
        if (ret != cmemlink.MEMLINK_OK) {
            System.out.println("create queue error!" + ret);
            return;
        }
        
        for (i = 0; i < 100; i++) {
            byte []value = String.format("%010d", i).getBytes();
            ret = m.lpush(key, value, "8:1:1");
            if (ret != cmemlink.MEMLINK_OK) {
                System.out.println("lpush error:" + ret);
                return;
            }
        }
        for (i = 100; i < 200; i++) {
            byte []value = String.format("%010d", i).getBytes();
            ret = m.rpush(key, value, "8:1:0");
            if (ret != cmemlink.MEMLINK_OK) {
                System.out.println("rpush error:" + ret);
                return;
            }
        }
        
        MemLinkCount count = new MemLinkCount();
        ret = m.count(key, "", count);
        if (ret == cmemlink.MEMLINK_OK) {
            System.out.println("count:" + count.getVisible_count());
            if (count.getVisible_count() + count.getTagdel_count() != 200) {
                System.out.println("count error!");
            }
        }

        for (i = 0; i < 20; i++) {
            MemLinkResult result = new MemLinkResult();
            ret = m.lpop(key, 2, result);
            if (ret != cmemlink.MEMLINK_OK) {
                System.out.println("lpop error:" + ret);
                return;
            }
        }
        for (i = 0; i < 20; i++) {
            MemLinkResult result = new MemLinkResult();
            ret = m.rpop(key, 2, result);
            if (ret != cmemlink.MEMLINK_OK) {
                System.out.println("rpop error:" + ret);
                return;
            }
        }

        MemLinkCount count2 = new MemLinkCount();
        ret = m.count(key, "", count2);
        if (ret == cmemlink.MEMLINK_OK) {
            System.out.println("count:" + count2.getVisible_count());
            if (count2.getVisible_count() + count2.getTagdel_count() != 120) {
                System.out.println("count error!");
            }
        }

        m.destroy();
    }

    public static void testSortList()
    {
        MemLinkClient m = new MemLinkClient("127.0.0.1", 11001, 11002, 10);
        int i;
		String name = "table2";
        String key = name + ".haha";
        
        m.rmkey(key);

        int ret;
		int num = 100;

        //ret = m.createSortList(key, 10, "4:3:1", cmemlink.MEMLINK_VALUE_STRING);
        ret = m.createTableSortList(name, 4, "4:3:1", cmemlink.MEMLINK_VALUE_INT);
        if (ret != cmemlink.MEMLINK_OK) {
            System.out.println("create queue error!" + ret);
            return;
        }
       
        List<byte[]> values = new ArrayList<byte[]>();

        for (i = 0; i < num; i++) {
            //byte []value = String.format("%010d", i).getBytes();
            byte []value = ByteBuffer.allocate(4).putInt(i).array();
            values.add(value);
        }

        Collections.shuffle(values);

        for (i = 0; i < num; i++) {
            //System.out.println(new String(values.get(i)));
            System.out.println(byteArrayToInt(values.get(i), 0));
        }

        for (i = 0; i < num; i++) {
            ret = m.sortListInsert(key, values.get(i), "7:2:1");
            if (ret != cmemlink.MEMLINK_OK) {
                System.out.println("insert error: " + ret);
                return;
            }
        }

        MemLinkResult rs = new MemLinkResult();
        ret = m.sortListRangeVisible(key, values.get(0), values.get(10), "", rs);
        if (cmemlink.MEMLINK_OK != ret) {
            System.out.println("range error!");
            return;
        }
        System.out.println("count:" + rs.getCount());

        MemLinkItem item = rs.getItems();
        while(item != null) {
			//String val = new String(item.getValue());
			int val = byteArrayToInt(item.getValue(), 0);
            System.out.println(val);
            item = item.getNext();
        }
        rs.delete();
        
        byte []first = String.format("%010d", 0).getBytes();
        byte []last = String.format("%010d", 100).getBytes();

        byte []start = String.format("%010d", 15).getBytes();
        byte []end   = String.format("%010d", 33).getBytes();

        MemLinkCount count = new MemLinkCount();
        ret = m.sortListCount(key, first, last, "", count);
        if (ret != cmemlink.MEMLINK_OK) {
            System.out.println("count error: " + ret);
            return; 
        }
        System.out.println(count.getVisible_count());

        ret = m.sortListCount(key, start, end, "", count);
        if (ret != cmemlink.MEMLINK_OK) {
            System.out.println("count error: " + ret);
            return; 
        }
        if (count.getVisible_count() != 18) {
            System.out.println("count error: " + ret);
            return;
        }

        ret = m.sortListDelete(key, cmemlink.MEMLINK_VALUE_ALL, start, end, "");
        if (ret != cmemlink.MEMLINK_OK) {
            System.out.println("delete error: " + ret);
            return; 
        }

        ret = m.sortListCount(key, first, last, "", count);
        if (ret != cmemlink.MEMLINK_OK) {
            System.out.println("count error: " + ret);
            return; 
        }
        if (count.getVisible_count() != 100 - 18) {
            System.out.println("count error: " + ret);
        }

		ret = m.clean(key);
		if (ret != cmemlink.MEMLINK_OK) {
		    System.out.println("clean error: " + ret);
			return;
		}

        m.destroy();
    }
	
    public static void test()
    {
        MemLinkClient m = new MemLinkClient("127.0.0.1", 11001, 11002, 10);
        
        int ret;
        int num = 100;
        int valuelen = 12;
        String attrformat = "4:3:1";
		String name = "table3";
		String key = name + ".haha";

        ret = m.rmkey(key);
        if (ret != cmemlink.MEMLINK_OK) {
            System.out.println("rmkey error: " + ret);
            return;
        }

	    ret = m.createTableList(key, valuelen, attrformat);
        if (cmemlink.MEMLINK_OK != ret) {
            System.out.println("create error:" + ret);
            return;
        }else{
            System.out.println("create ok:" + key); 
        }
        
        int i = 0;
        //String value = "012345678912";
        for (i = 0; i < num; i++) {
            byte []value = String.format("%010d", i).getBytes();
            ret = m.insert(key, value, "8:3:1", 0);
            if (cmemlink.MEMLINK_OK != ret) {
                System.out.println("insert error!");
                return;
            }
        }
        
        System.out.println("insert 200 ok!"); 
        
        MemLinkStat stat = new MemLinkStat();
        ret = m.stat(key, stat);
        if (cmemlink.MEMLINK_OK != ret) {
            System.out.println("stat error!");
            return;
        }
        //System.out.println("data: %d, used: %d", stat.getData(), stat.getData_used());

        if (stat.getData() != num || stat.getData_used() != num) {
            System.out.println("stat result error!");
            return;
        }
        stat.delete();
        
        MemLinkResult rs = new MemLinkResult();
        ret = m.range(key, cmemlink.MEMLINK_VALUE_VISIBLE, "", 0, 100, rs);
        if (cmemlink.MEMLINK_OK != ret) {
            System.out.println("range error!");
            return;
        }
        if (rs.getCount() != num) {
            System.out.println("err count:" + rs.getCount());
            return;
        }
        i = num; 
        MemLinkItem item = rs.getItems();
        while(i > 0) {
            i--;
            String value = String.format("%010d", i);
			String value2 = new String(item.getValue());	
            System.out.println(value2);
            if (!value.equals(value2)){
                System.out.println("item.value: " + value2);
                return;
            }
            item = item.getNext();
        }
        rs.delete();

        System.out.println("move first value to next slot:");
        ret = m.move(key, "0000000000".getBytes(), 1);
        if (ret != cmemlink.MEMLINK_OK) {
            System.out.println("move error!");
            return;
        }

        rs = new MemLinkResult();
        //ret = m.rangeVisible(key, "", 0, 100, rs);
        ret = m.range(key, cmemlink.MEMLINK_VALUE_VISIBLE, "", 0, 100, rs);
        if (cmemlink.MEMLINK_OK != ret) {
            System.out.println("range error!");
            return;
        }
        if (rs.getCount() != num) {
            System.out.println("err count:" + rs.getCount());
            return;
        }
        i = num; 
        item = rs.getItems();
        while(i > 0) {
            i--;
            String value = String.format("%010d", i);
			String value2 = new String(item.getValue());
            System.out.println(value2);
            if (!value.equals(value2)){
                System.out.println("item.value: " + value2);
                return;
            }
            item = item.getNext();
        }
        rs.delete();

        System.out.print("modify the first value\'s attr:");
		ret = m.attr(key, "0000000000".getBytes(), "1:1:1");
        if (ret != cmemlink.MEMLINK_OK) {
            System.out.println("attr error!");
            return;
        }
        rs = new MemLinkResult();
        //ret = m.rangeVisible(key, "", 0, 100, rs);
        ret = m.range(key, cmemlink.MEMLINK_VALUE_VISIBLE, "", 0, 100, rs);
        if (cmemlink.MEMLINK_OK != ret) {
            System.out.println("range error!");
            return;
        }
        if (rs.getCount() != num) {
            System.out.println("err count:" + rs.getCount());
            return;
        }
        i = num; 
        item = rs.getItems();
        while(i > 0) {
            i--;
            String value = String.format("%010d", i);
			String value2 = new String(item.getValue());
            System.out.println(value2);
            if (!value.equals(value2)){
                System.out.println("item.value: " + value2);
                return;
            }
            item = item.getNext();
        }
        rs.delete();

        System.out.print("tag the first value deleted");
		ret = m.tag(key, "0000000000".getBytes(), 1);
        if (ret != cmemlink.MEMLINK_OK) {
            System.out.println("attr error!");
            return;
        }
        rs = new MemLinkResult();
        //ret = m.rangeTagdel(key, "", 0, 100, rs);
        ret = m.range(key, cmemlink.MEMLINK_VALUE_TAGDEL, "", 0, 100, rs);
        if (cmemlink.MEMLINK_OK != ret) {
            System.out.println("range error!");
            return;
        }
        if (rs.getCount() != num) {
            System.out.println("err count:" + rs.getCount());
            return;
        }
        i = num; 
        item = rs.getItems();
        while(i > 0) {
            i--;
            String value = String.format("%010d", i);
			String value2 = new String(item.getValue());
            System.out.println(value2);
            if (!value.equals(value2)){
                System.out.println("item.value: " + value2);
                return;
            }
            item = item.getNext();
        }
        rs.delete();

        MemLinkCount count = new MemLinkCount();
        ret = m.count(key, "8:3:1", count);
        if (cmemlink.MEMLINK_OK != ret) {
            System.out.println("count error!");
            return;
        }
        
        System.out.println("count.visible:" + count.getVisible_count() + "\ncount.tagdel:" + count.getTagdel_count());
        count.delete(); 

        ret = m.dump();
		if (ret != cmemlink.MEMLINK_OK) {
		    System.out.println("dump error: " + ret);
			return;
		}

        m.destroy();

    }
	
	public static void testInfo()
	{
        MemLinkClient m = new MemLinkClient("127.0.0.1", 11001, 11002, 10);

	    MemLinkRcInfo rcinfo = new MemLinkRcInfo();
	    int ret;


        System.out.println("read connection info:");
		ret = m.readConnInfo(rcinfo);
		if (ret != cmemlink.MEMLINK_OK) {
		    System.out.println("read conn info error: " + ret);
			return;
		}

		System.out.println("conncount: " + rcinfo.getCount());
		MemLinkRcItem rcitem = rcinfo.getItems();
        while(rcitem != null) {
            System.out.println("client ip: " + rcitem.getClient_ip());
            System.out.println("client port: " + rcitem.getPort());
            rcitem = rcitem.getNext();
        }

		rcinfo.delete();

	    MemLinkWcInfo wcinfo = new MemLinkWcInfo();
        System.out.println("write connection info:");
		ret = m.writeConnInfo(wcinfo);
		if (ret != cmemlink.MEMLINK_OK) {
		    System.out.println("write conn info error: " + ret);
			return;
		}

		System.out.println("conncount: " + wcinfo.getCount());
		MemLinkWcItem wcitem = wcinfo.getItems();
        while(wcitem != null) {
            System.out.println("client ip: " + wcitem.getClient_ip());
            System.out.println("client port: " + wcitem.getPort());
            wcitem = wcitem.getNext();
        }

		wcinfo.delete();

	    MemLinkScInfo scinfo = new MemLinkScInfo();
        System.out.println("sync connection info:");
		ret = m.syncConnInfo(scinfo);
		if (ret != cmemlink.MEMLINK_OK) {
		    System.out.println("sync conn info error: " + ret);
			return;
		}

		System.out.println("conncount: " + scinfo.getCount());
		MemLinkScItem scitem = scinfo.getItems();
        while(scitem != null) {
            System.out.println("client ip: " + scitem.getClient_ip());
            System.out.println("client port: " + scitem.getPort());
            scitem = scitem.getNext();
        }

		scinfo.delete();
	}

	public static void testInsertMkv()
	{
        MemLinkClient m = new MemLinkClient("127.0.0.1", 11001, 11002, 10);

        int valuelen = 12;
        String attrformat = "4:3:1";
		String name = "table4";
		String keyname = name + ".haha";
		int ret;

        ret = m.rmkey(keyname);
        if (ret != cmemlink.MEMLINK_OK) {
            System.out.println("rmkey error: " + ret);
        }

	    ret = m.createTableList(name, valuelen, attrformat);
        if (cmemlink.MEMLINK_OK != ret) {
            System.out.println("create error:" + ret);
            return;
        }else{
            System.out.println("create ok:" + keyname); 
        }

	    MemLinkInsertMkv mkv = new MemLinkInsertMkv();
		MemLinkInsertKey key = new MemLinkInsertKey(keyname);

		int i;
		for (i = 0; i < 10; i++) {
            String value = String.format("%010d", i);
	        MemLinkInsertVal val = new MemLinkInsertVal(value.getBytes(), "1:1:1", 0);
			key.add(val);
		}

        mkv.add(key);
	    ret = m.insertMKV(mkv);
		if (ret != cmemlink.MEMLINK_OK) {
		    System.out.println("insertMKV error: " + ret);
			return;
		}

        MemLinkResult rs = new MemLinkResult();
        ret = m.range(keyname, cmemlink.MEMLINK_VALUE_VISIBLE, "", 0, 10, rs);
        if (cmemlink.MEMLINK_OK != ret) {
            System.out.println("range error!");
            return;
        }
        i = 10; 
        MemLinkItem item = rs.getItems();
        while(i > 0) {
            i--;
            String value = String.format("%010d", i);
            System.out.println(item.getValue());
            if (!value.equals(item.getValue())){
                System.out.println("item.value: " + item.getValue());
                return;
            }
            item = item.getNext();
        }
        rs.delete();

		mkv.delete();
	}
	
	public static void testBinary()
	{
		MemLinkClient m = new MemLinkClient("127.0.0.1", 11001, 11002, 10);

        int valuelen = 4;
        String attrformat = "4:3:1";
		String name = "table5";
		String keyname = name + ".haha";
		int ret;

        ret = m.rmkey(keyname);
        if (ret != cmemlink.MEMLINK_OK) {
            System.out.println("rmkey error: " + ret);
        }

	    ret = m.createTableList(name, valuelen, attrformat);
        if (cmemlink.MEMLINK_OK != ret) {
            System.out.println("create error:" + ret);
            return;
        }

		int i;
		for (i = 0; i < 10; i++) {
			ret = m.insert(keyname, ByteBuffer.allocate(4).putInt(i).array(), "8:2:1", 0);
			if (ret != cmemlink.MEMLINK_OK) {
				System.out.println("insert error:" + ret);
			}
		}

		MemLinkResult rs = new MemLinkResult();
        ret = m.range(keyname, cmemlink.MEMLINK_VALUE_VISIBLE, "", 0, 10, rs);
        if (cmemlink.MEMLINK_OK != ret) {
            System.out.println("range error!");
            return;
        }
		System.out.println("count: " + rs.getCount());
        MemLinkItem item = rs.getItems();
        while(item != null) {
            //String value = String.format("%010d", i);
			byte []value = item.getValue();
            System.out.println("item.value: " + byteArrayToInt(value, 0));
            item = item.getNext();
        }
        rs.delete();


	}

	public static int byteArrayToInt(byte[] b, int offset) {
        int value = 0;
        for (int i = 0; i < 4; i++) {
            int shift = (4 - 1 - i) * 8;
            value += (b[i + offset] & 0x000000FF) << shift;
        }
        return value;
    }

	public static void main(String [] args)
	{
        //testQueue();			
        //testSortList();
		//test();
		//testInsertMkv();
		testBinary();
    }
}
