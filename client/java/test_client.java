import com.googlecode.memlink.*;
import java.util.*;
import java.lang.*;

public class test_client
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

	public static void test_create_table(MemLinkClient m, String name)
	{
		int ret;
		int valuelen = 12;
		String maskformat = "4:3:1";

		ret = m.createTableList(name, valuelen, maskformat);
		if (0 != ret){
			System.out.println("create error:" + ret);
			return;
		}else{
			System.out.println("create " + name); 
		}
	}

	public static void test_insert(MemLinkClient m, String key)
	{
		int i = 0;
		int ret = 0;
		int num = 200;
		//String value = "012345678912";

		for (i = 0; i < num; i++) {
			String value = String.format("%012d", i);
			ret = m.insert(key, value.getBytes(), "8:3:1", 0);
			if (0 != ret)
			{
				System.out.println("errr insert!");
				return;
			}
		}

		System.out.println("insert 200");               
	}

	public static void test_stat(MemLinkClient m, String key)
	{        
		int ret;
		MemLinkStat stat = new MemLinkStat();
		ret = m.stat(key, stat);
		if (cmemlink.MEMLINK_OK != ret)
		{
			System.out.println("errr stat!");
			return;
		}
		if (stat.getData() != 200 || stat.getData_used() != 200)
		{    
			System.out.println("errr stat result!");
			return;
		}
	}

	public static void test_range(MemLinkClient m, String key)
	{        
		int ret;
		MemLinkResult rs = new MemLinkResult();
		ret = m.range(key, cmemlink.MEMLINK_VALUE_VISIBLE, "", 0, 200, rs);
		if (cmemlink.MEMLINK_OK != ret)
		{
			System.out.println("err range!");
			return;
		}
		System.out.println("range count: " + rs.getCount());
		MemLinkItem item = rs.getItems();
		while (null != item)
		{
			String v = new String(item.getValue());
			System.out.println("value: " + v + " length:" + v.length());
			item = item.getNext();
		}
	}

	public static void test_count(MemLinkClient m, String key)
	{
		int ret;

		MemLinkCount count = new MemLinkCount();
		ret = m.count(key, "8:3:1", count);
		if (cmemlink.MEMLINK_OK != ret)
		{
			System.out.println("err count!");
			return;
		}
		System.out.println("count.visible: " + count.getVisible_count() + "\ncount.tagdel: " + count.getTagdel_count());                
	}


	public static void test_move(MemLinkClient m, String key)
	{
		int ret;
		int i = 0;

		String value = String.format("%012d", i); //the last one
		ret = m.move(key, value.getBytes(), 0); //move it to first
		if (0 != ret)
		{
			System.out.println("err tag");
			return;
		}
	}

	public static void test_tag(MemLinkClient m, String key)
	{
		int ret;
		int i = 100;

		String value = String.format("%012d", i);
		ret = m.tag(key, value.getBytes(), cmemlink.MEMLINK_TAG_DEL); //tag value
		if (0 != ret)
		{
			System.out.println("err tag");
			return;
		}
	}

	public static void test_del(MemLinkClient m, String key)
	{
		int ret;
		int i = 199;

		String value = String.format("%012d", i);
		ret = m.tag(key, value.getBytes(), cmemlink.MEMLINK_TAG_DEL);
		if (0 != ret)
		{
			System.out.println("err tag");
			return;
		}
	}

	public static void main(String [] args)
	{
		//String str = System.getProperty("java.library.path");
		//System.out.println(str);

		MemLinkClient m = new MemLinkClient("127.0.0.1", 11001, 11002, 10);
		String name = "testtable";
		String key = name + ".haha";

		test_create_table(m, name);
		test_insert(m, key);
		test_stat(m, key);
		System.out.println("\nrange1: ");
		test_range(m, key);
		test_count(m, key);
		//test_update(m, key);
		test_tag(m, key);
		test_del(m, key);
		System.out.println("\nrange2: ");
		test_range(m, key);

		m.destroy();
	}
}

