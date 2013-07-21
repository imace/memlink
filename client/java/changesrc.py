# coding: utf-8
import os, sys
import time

class ChangeSourceItem (object):
    name = ''
    actions = None

    def __init__(self):
        self.actions = []

    def add_action(self, act):
        self.actions.append(act)

    def do(self):
        filename = 'com/googlecode/memlink/' + self.name
        #print 'filename:', filename

        f = open(filename, 'r')
        lines = f.readlines()
        f.close()
        
        for a in self.actions:
            func  = getattr(self, 'doaction_' + a.name)
            lines = func(a, lines)         
        
        #os.rename(filename, filename+'.%f' % time.time())  

        f = open(filename , 'w')
        f.write(''.join(lines))
        f.close()

    def doaction_addstat(self, action, lines):
        funcstr = action.cond.func
        if funcstr:
            funcstr = ' ' + funcstr + '(' 
        foundfunc = False
        for i in range(0, len(lines)):
            line = lines[i]
            #print 'line:', line
            if funcstr:
                #if foundfunc:
                    #print line
                if line.find(funcstr) >= 0:
                    #print 'found:', funcstr, 'next find:', action.cond.s
                    foundfunc = True

                if foundfunc and line.startswith('  }'):
                    foundfunc = False
                    #print 'found func end'
                    break
                
                if foundfunc and line.find(action.cond.s) >= 0:
                    #print 'action.s:', action.s
                    #print 'insert:', action.s
                    lines.insert(i+1, action.s)
                    break
            else:
                if line.find(action.cond.s) >= 0:
                    #print 'insert:', action.s
                    lines.insert(i+1, action.s)
                    break
        return lines

    def doaction_addfunc(self, action, lines):
        for i in range(len(lines)-1, -1, -1):        
            line = lines[i]
            if line.startswith('}'):
                lines.insert(i, action.s)
                break
        return lines

    def doaction_repstat(self, action, lines):
        funcstr = action.cond.func
        if funcstr:
            funcstr = ' ' + funcstr + '(' 

        foundfunc = False
        for i in range(0, len(lines)):
            line = lines[i]
            if funcstr:
                if line.find(funcstr) >= 0:
                    #print 'found:', funcstr, 'next find:', action.cond.s
                    foundfunc = True

                if foundfunc and line.startswith('  }'):
                    foundfunc = False
                    #print 'found func end'
                    break

                if foundfunc and line.find(action.cond.s) >= 0:
                    lines[i] = '//' + lines[i]
                    lines.insert(i+1, action.s)
                    break
            else:
                if line.find(action.cond.s) >= 0:
                    lines[i] = '//' + lines[i]
                    lines.insert(i+1, action.s)
                    break

        return lines

    def doaction_rmstat(self, action, lines):
        funcstr = action.cond.func
        if funcstr:
            funcstr = ' ' + funcstr + '(' 

        for i in range(0, len(lines)):
            line = lines[i]
            if funcstr:
                if line.find(funcstr) >= 0:
                    foundfunc = True

                if foundfunc and line.startswith('  }'):
                    break

                if foundfunc and line.find(s) >= 0:
                    lines[i] = '//' + lines[i]
                    break
        return lines


class FindCond:
    func = ''
    s    = ''
    line = -1

class Action (object):
    # addstat, rmstat, repstat, addfunc
    name = ''

    def __init__(self, name, find, newstr):
        self.name = name
        self.cond = find
        self.s = newstr

class ChangeMemLinkResult (ChangeSourceItem):
    name = 'MemLinkResult.java'
    actions = []

    def __init__(self):
        cond = FindCond()
        cond.func = 'void delete'
        cond.s = 'if (swigCPtr != 0) {'

        act = Action('addstat', cond, '      cmemlink.memlink_result_free(this);\n')
        self.add_action(act)


class ChangeMemLinkRcInfo (ChangeSourceItem):
    name = 'MemLinkRcInfo.java'
    actions = []

    def __init__(self):
        cond = FindCond()
        cond.func = 'void delete'
        cond.s = 'if (swigCPtr != 0) {'

        act = Action('addstat', cond, '      cmemlink.memlink_rcinfo_free(this);\n')
        self.add_action(act)


class ChangeMemLinkScInfo (ChangeSourceItem):
    name = 'MemLinkScInfo.java'
    actions = []

    def __init__(self):
        cond = FindCond()
        cond.func = 'void delete'
        cond.s = 'if (swigCPtr != 0) {'

        act = Action('addstat', cond, '      cmemlink.memlink_scinfo_free(this);\n')
        self.add_action(act)

class ChangeMemLinkWcInfo (ChangeSourceItem):
    name = 'MemLinkWcInfo.java'
    actions = []

    def __init__(self):
        cond = FindCond()
        cond.func = 'void delete'
        cond.s = 'if (swigCPtr != 0) {'

        act = Action('addstat', cond, '      cmemlink.memlink_wcinfo_free(this);\n')
        self.add_action(act)

class ChangeMemLinkInsertVal (ChangeSourceItem):
    name = 'MemLinkInsertVal.java'
    actions = []

    def __init__(self):
        #cond = FindCond()
        #cond.s = 'this(cmemlinkJNI.new_MemLinkInsertVal(), true);'
        #self.add_action(Action('repstat', cond, '    this(cmemlinkJNI.memlink_ival_create(value, attrstr, pos), true);\n'))

        s = '''public MemLinkInsertVal(byte []value, String attrstr, int pos) {
    this(cmemlinkJNI.memlink_ival_create(value, attrstr,pos), true);
  }'''
        self.add_action(Action('addfunc', None, s))

class ChangeMemLinkInsertKey (ChangeSourceItem):
    name = 'MemLinkInsertKey.java'
    actions = []

    def __init__(self):
        #cond = FindCond()
        #cond.s = 'this(cmemlinkJNI.new_MemLinkInsertKey(), true);'
        #self.add_action(Action('repstat', cond, '    this(cmemlinkJNI.memlink_ikey_create(key, key.length()), true);\n'))

        s = '''  public MemLinkInsertKey(String table, String key) {
    this(cmemlinkJNI.memlink_ikey_create(table, key, key.length()), true);
  }'''
        self.add_action(Action('addfunc', None, s))

        s = '''  public int add(MemLinkInsertVal valobj) {
      return cmemlinkJNI.memlink_ikey_add_value(swigCPtr, this, MemLinkInsertVal.getCPtr(valobj), valobj);\n  }\n'''
        self.add_action(Action('addfunc', None, s))


class ChangeMemLinkInsertMkv (ChangeSourceItem):
    name = 'MemLinkInsertMkv.java'
    actions = []

    def __init__(self):
        cond = FindCond()
        cond.func = 'void delete'
        cond.s = 'if (swigCPtr != 0) {'

        act = Action('addstat', cond, '      cmemlink.memlink_imkv_destroy(this);\n')
        self.add_action(act)

        cond = FindCond()
        cond.s = 'this(cmemlinkJNI.new_MemLinkInsertMkv(), true);'
        self.add_action(Action('repstat', cond, '    this(cmemlinkJNI.memlink_imkv_create(), true);\n'))


        s = '''  public int add(MemLinkInsertKey keyobj) {
    return cmemlinkJNI.memlink_imkv_add_key(swigCPtr, this, MemLinkInsertKey.getCPtr(keyobj), keyobj);\n
  }\n'''
        self.add_action(Action('addfunc', None, s))



def main():
    for k,c in globals().iteritems():
        if k.startswith('ChangeMemLink'):
            x = c()
            x.do()

if __name__ == '__main__':
    main()

 
