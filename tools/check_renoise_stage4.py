#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Stage 4 integration in the dedicated Renoise scratch host.

Requires X11/XTest, ImageMagick import, and the playback recorder dependencies.
Wait for the main song window before connecting: the host's song.rendering API
can crash during startup/recovery. Does not operate any unrelated window/song.
"""
import ctypes as C,json,sys,time
from pathlib import Path
X=C.CDLL('libX11.so.6'); T=C.CDLL('libXtst.so.6')
D=C.c_void_p; U=C.c_ulong; I=C.c_int
X.XOpenDisplay.argtypes=[C.c_char_p];X.XOpenDisplay.restype=D
X.XDefaultRootWindow.argtypes=[D];X.XDefaultRootWindow.restype=U
X.XQueryTree.argtypes=[D,U,C.POINTER(U),C.POINTER(U),C.POINTER(C.POINTER(U)),C.POINTER(C.c_uint)]
X.XFetchName.argtypes=[D,U,C.POINTER(C.c_char_p)]
X.XGetGeometry.argtypes=[D,U,C.POINTER(U),C.POINTER(I),C.POINTER(I),C.POINTER(C.c_uint),C.POINTER(C.c_uint),C.POINTER(C.c_uint),C.POINTER(C.c_uint)]
X.XFree.argtypes=[D]
X.XTranslateCoordinates.argtypes=[D,U,U,I,I,C.POINTER(I),C.POINTER(I),C.POINTER(U)]
X.XInternAtom.argtypes=[D,C.c_char_p,I];X.XInternAtom.restype=U
X.XGetWindowProperty.argtypes=[D,U,U,C.c_long,C.c_long,I,U,C.POINTER(U),C.POINTER(I),C.POINTER(U),C.POINTER(U),C.POINTER(C.POINTER(C.c_ubyte))]
X.XFlush.argtypes=[D]
X.XSetInputFocus.argtypes=[D,U,I,U]
X.XRaiseWindow.argtypes=[D,U]
X.XMoveWindow.argtypes=[D,U,I,I]
T.XTestFakeMotionEvent.argtypes=[D,I,I,I,U];T.XTestFakeButtonEvent.argtypes=[D,C.c_uint,I,U]
T.XTestFakeKeyEvent.argtypes=[D,C.c_uint,I,U]
X.XStringToKeysym.argtypes=[C.c_char_p];X.XStringToKeysym.restype=U
X.XKeysymToKeycode.argtypes=[D,U];X.XKeysymToKeycode.restype=C.c_uint
d=X.XOpenDisplay(None)
assert d,'No X display'
root=X.XDefaultRootWindow(d)
def geom(w):
 r=U();x=I();y=I();width=C.c_uint();h=C.c_uint();border=C.c_uint();depth=C.c_uint()
 X.XGetGeometry(d,w,C.byref(r),C.byref(x),C.byref(y),C.byref(width),C.byref(h),C.byref(border),C.byref(depth))
 xx=I();yy=I();child=U();X.XTranslateCoordinates(d,w,root,0,0,C.byref(xx),C.byref(yy),C.byref(child))
 return [xx.value,yy.value,width.value,h.value]
def pid(w):
 typ=U();fmt=I();n=U();left=U();data=C.POINTER(C.c_ubyte)()
 X.XGetWindowProperty(d,w,X.XInternAtom(d,b'_NET_WM_PID',0),0,1,0,6,C.byref(typ),C.byref(fmt),C.byref(n),C.byref(left),C.byref(data))
 out=C.cast(data,C.POINTER(U))[0] if n.value else 0
 if data:X.XFree(data)
 return out
WINDOWS=[]
def tree(w,depth=0):
 name=C.c_char_p();X.XFetchName(d,w,C.byref(name))
 title=name.value.decode(errors='replace') if name.value else ''
 if name:X.XFree(name)
 if title: WINDOWS.append({'window':w,'pid':pid(w),'title':title,'geometry':geom(w),'depth':depth})
 r=U();parent=U();children=C.POINTER(U)();n=C.c_uint()
 if X.XQueryTree(d,w,C.byref(r),C.byref(parent),C.byref(children),C.byref(n)):
  for i in range(n.value):tree(children[i],depth+1)
  if children:X.XFree(children)
def move(x,y):T.XTestFakeMotionEvent(d,-1,int(x),int(y),0);X.XFlush(d)
def button(down):T.XTestFakeButtonEvent(d,1,int(down),0);X.XFlush(d)
def key(name,down):T.XTestFakeKeyEvent(d,X.XKeysymToKeycode(d,X.XStringToKeysym(name.encode())),int(down),0);X.XFlush(d)
def click(x,y):move(x,y);button(True);time.sleep(.06);button(False);time.sleep(.15)

import argparse,subprocess
from check_renoise_stage3 import Stage3, ROOT

class Stage4(Stage3):
    def capture(self, name, seconds, action=None):
        return super().capture(name, seconds, action, settle_seconds=.1)
    def __init__(self, process_id):
        WINDOWS.clear();tree(root)
        main=[w for w in WINDOWS if w['pid']==process_id and w['title'].endswith(' - Renoise (x86_64)') and '.xrns' in w['title']]
        assert len(main)==1 and 'sidsynth-' in main[0]['title'].lower(), 'Wait for the scratch song main window; dismiss recovery before running'
        time.sleep(1)
        super().__init__(process_id)
        self.transcript=ROOT/'stage4-mcp-transcript.jsonl'
        self.results.pop('legacy_schema2_loaded',None)
        self.window=None
        self.results['pid']=process_id
        self.monitor_links=[]

    def detach_monitor(self):
        objects=json.loads(subprocess.check_output(['pw-dump']))
        clients={o['id'] for o in objects if str(o.get('info',{}).get('props',{}).get('application.process.id'))==str(self.pid)}
        nodes={o['id'] for o in objects if o.get('info',{}).get('props',{}).get('client.id') in clients}
        self.monitor_links=[(o['info']['output-port-id'],o['info']['input-port-id']) for o in objects
                            if o.get('type')=='PipeWire:Interface:Link' and o['info']['output-node-id'] in nodes]
        for output,input_ in self.monitor_links:subprocess.run(['pw-link','-d',str(output),str(input_)],check=True)
        self.results['monitor_links_temporarily_detached']=self.monitor_links

    def silence(self):
        self.call('transport_stop')
        for v in range(1,4):self.parameter('Voice '+str(v)+' manual hold',0)
        # A mode edge clears host-held ownership even if transport was stopped.
        self.parameter('Play mode',0);time.sleep(.08);self.parameter('Play mode',1);time.sleep(.08)

    def restore_monitor(self):
        self.silence()
        for output,input_ in self.monitor_links:subprocess.run(['pw-link',str(output),str(input_)],check=True)
        self.results['monitor_restored_after_silence']=True

    def window_info(self):
        WINDOWS.clear();tree(root)
        windows=[w for w in WINDOWS if w['pid']==self.pid and w['title']=='SIDSynth: SIDSynth']
        assert len(windows)==1, 'Cannot identify this scratch process Rack editor'
        return windows[0]

    def focus(self):
        info=self.window_info();self.window=info['window']
        # Keep the test editor on the X11 input surface when it grows.
        X.XMoveWindow(d,self.window,80,80);X.XFlush(d);time.sleep(.2)
        X.XRaiseWindow(d,self.window);X.XSetInputFocus(d,self.window,2,0);X.XFlush(d)
        time.sleep(.25)
        return info

    def point(self,x,y):
        info=self.window_info();gx,gy,w,h=info['geometry'];scale=w/1120
        return round(gx+x*scale),round(gy+y*scale)

    def click(self,x,y):
        self.focus();move(*self.point(x,y));time.sleep(.1);button(True);time.sleep(.08);button(False);time.sleep(.2)

    def keys(self,*names):
        for name in names:key(name,True);key(name,False)
        time.sleep(.12)

    def choose_zoom(self,index):
        self.click(1052,40)
        r=U();parent=U();children=C.POINTER(U)();n=C.c_uint()
        X.XQueryTree(d,root,C.byref(r),C.byref(parent),C.byref(children),C.byref(n))
        menus=[]
        for i in range(n.value):
            w=children[i]
            if pid(w)==self.pid:
                x,y,width,height=geom(w)
                if 50<=width<=260 and 50<=height<=260:menus.append((x,y,width,height))
        if children:X.XFree(children)
        assert len(menus)==1,'Cannot identify size popup'
        x,y,width,height=menus[0]
        click(x+width//2,y+1+round((index+.5)*(height-2)/4))
        time.sleep(.3)

    def type_number(self,x,y,value):
        self.click(x,y);key('Control_L',True);self.keys('a');key('Control_L',False)
        for c in str(value):key(c,True);key(c,False)
        self.keys('Return')

    def value(self,name):
        return next(p for p in self.host('inspect')['instruments'][0]['parameters'] if p['name']==name)

    def screenshot(self,name):
        info=self.window_info()
        path=ROOT/(name+'.png')
        subprocess.run(['import','-window',str(info['window']),str(path)],check=True)
        return {'file':str(path),'geometry':info['geometry']}

    def run(self):
        self.default_patch()
        self.pattern([])
        self.host('editor',visible=False);time.sleep(.15)
        self.host('editor',visible=True);time.sleep(.3)
        self.focus()
        self.results['rack_100']=self.screenshot('stage4-rack-host-100')
        self.click(210,167)
        assert self.value('Voice 1 triangle')['value']==1 and self.value('Voice 1 saw')['value']==1
        self.results['independent_waveform_click']=True
        self.click(210,167)
        self.type_number(940,280,820)
        assert self.value('Filter cutoff')['text'].strip()=='820 reg'
        self.results['numeric_entry']=self.value('Filter cutoff')
        # Explicit Voice 2 target, with CH1 shared by V1 and V2. Keyboard
        # navigation in the dropdown is exercised alongside mouse audition.
        self.parameter('Voice 2 MIDI channel',0)
        self.click(112,720);self.keys('Home','Down','Return')
        self.results['target_dropdown']=self.screenshot('stage4-target-voice2')
        def press():
            self.focus();move(*self.point(566.5,735));button(True)
        try:
            audio=self.capture('stage4-host-keyboard',2.2,press)
            assert self.tone(audio,.3,1.3,440)>.003,'Audition keyboard is silent/wrong pitch'
            self.results['gate_during_audition']=self.screenshot('stage4-gate-on')
        finally:button(False)
        # Manual frequency and Hold allow an unambiguous in-host Panic click.
        self.parameter('Voice 1 source',1)
        self.parameter('Voice 1 manual frequency',7493/65535)
        self.parameter('Voice 1 manual hold',0);time.sleep(.08)
        self.click(698,168)
        assert self.value('Voice 1 manual hold')['value']==1
        held=self.capture('stage4-host-ui-hold',2)
        assert self.tone(held,.3,1.3,440)>.003
        self.results['manual_editor']=self.screenshot('stage4-manual-hold')
        quiet=self.capture('stage4-host-ui-panic',2,lambda:self.click(1030,102))
        assert self.rms(quiet,1.0,1.9)<.0003,'UI Panic failed to silence manual hold'
        assert self.value('Voice 1 manual hold')['value']==1,'Panic changed saved patch parameters'
        self.results['ui_panic_silent_rms']=self.rms(quiet,1.0,1.9)
        self.click(900,696)
        self.results['advanced']=self.screenshot('stage4-advanced-host')
        self.click(730,530)
        assert self.value('Voice 1 test / reset')['value']==1
        self.click(730,530)
        self.click(1035,456)
        self.results['advanced_test_click']=True
        self.finish_layout()

    def finish_layout(self):
        self.host('editor',visible=False);time.sleep(.15)
        self.host('editor',visible=True);time.sleep(.3)
        # Zoom choices at four sizes, using keyboard navigation. Recompute
        # geometry after each host resize instead of assuming screen positions.
        self.results['sizes']=[]
        for index,scale in enumerate((.8,1,1.25,1.5)):
            self.choose_zoom(index)
            shot=self.screenshot('stage4-rack-host-'+str(round(scale*100)))
            shot['requested_size']=[round(1120*scale),round(780*scale)]
            assert all(abs(a-b)<=1 for a,b in zip(shot['geometry'][2:],shot['requested_size'])),shot
            self.results['sizes'].append(shot)
        # Return to a comfortable default and test native host reopen.
        self.choose_zoom(1)
        for _ in range(5):
            self.host('editor',visible=False);time.sleep(.1)
            self.host('editor',visible=True);time.sleep(.15)
            assert self.window_info()['geometry'][2:]==[1120,780]
        self.results['editor_reopen_cycles']=5
        self.finish_rack_recall()

    def finish_rack_recall(self):
        self.host('editor',visible=True);time.sleep(.2)
        self.parameter('Filter cutoff',820/2047)
        self.parameter('Voice 1 triangle',0)
        self.parameter('Voice 1 source',1)
        self.parameter('Voice 1 manual hold',0);time.sleep(.08)
        self.parameter('Voice 1 manual hold',1)
        self.click(1030,102) # Save an on-valued, disarmed manual Hold.
        self.host('save',name='sidsynth-stage4-recall')
        expected={p['name']:p['text'] for p in self.host('inspect')['instruments'][0]['parameters']}
        self.parameter('Filter cutoff',0);self.parameter('Voice 1 triangle',1)
        self.host('save',name='sidsynth-stage4-before-recall')
        self.host('load_song',name='sidsynth-stage4-recall')
        # Never inspect during document replacement. Wait for its main-window
        # title by X11 first, then allow one completed host idle cycle.
        deadline=time.monotonic()+15
        while True:
            WINDOWS.clear();tree(root)
            if any(w['pid']==self.pid and w['title'].startswith('sidsynth-stage4-recall.xrns - ') for w in WINDOWS):break
            assert time.monotonic()<deadline,'Recall main window did not settle'
            time.sleep(.2)
        time.sleep(1)
        actual={p['name']:p['text'] for p in self.host('inspect')['instruments'][0]['parameters']}
        assert actual==expected,'Rack patch recall differs'
        quiet=self.capture('stage4-host-recalled-silence',2)
        assert self.rms(quiet,.3,1.3)<.0003
        self.results['complete_recall']=True
        # Leave a simple, stopped, playable MIDI patch and pattern for the user.
        self.default_patch()
        self.parameter('Play mode',1)
        self.parameter('Voice 2 tuning',.4375)
        self.parameter('Voice 3 tuning',.5003646)
        self.pattern([(1,1,'A-5',0,1),(1,13,'OFF',0,1),(1,17,'C-6',0,1),(1,29,'OFF',0,1)])
        self.capture('stage4-host-audition',4.5,self.play)
        self.host('save',name='sidsynth-stage4-audition')
        self.host('editor',visible=True);time.sleep(.3);self.focus()
        self.results['final_editor']=self.screenshot('stage4-rack-final')
        self.results['audition_song']=str(ROOT/'sidsynth-stage4-audition.xrns')
        self.results['passed']=True
        print('PASS: real Rack clicks, numeric entry, keyboard, manual Hold/Panic, TEST, four sizes, five reopens, full recall and playback',flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--pid',type=int,required=True);parser.add_argument('--resume-layout',action='store_true');parser.add_argument('--resume-recall',action='store_true');args=parser.parse_args()
    checks=Stage4(args.pid)
    try:
        checks.detach_monitor()
        if args.resume_layout or args.resume_recall:
            retained=json.loads((ROOT/'stage4-host-results.json').read_text())
            assert retained['module_sha256']==checks.results['module_sha256']
            checks.results.update(retained);checks.results.pop('error',None)
            if args.resume_recall:checks.finish_rack_recall()
            else:checks.finish_layout()
        else:checks.run()
    except Exception as error:
        checks.results['passed']=False;checks.results['error']=repr(error);raise
    finally:
        button(False)
        checks.restore_monitor()
        (ROOT/'stage4-host-results.json').write_text(json.dumps(checks.results,indent=2)+'\n')
