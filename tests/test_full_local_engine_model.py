import unittest
from tools.sfn_full_local_engine_model import *

class FullEngineStorageTest(unittest.TestCase):
    def base(self):
        return EngineImage(seq=7,schedule_generation=3,voltage=VoltagePolicy(revision=4,enabled=True,low10=1800,high10=2500,hyst10=50,hold10=20,recovery=30,protected_mask=15,auto_restore=True),schedules=(ScheduleItem(0,1,1,0,NONE,480,True),ScheduleItem(1,1,1,0,NONE,1320,False)))
    def test_roundtrip_keeps_voltage_and_schedule_together(self):
        x=self.base(); self.assertEqual(decode(encode(x)),x)
    def test_voltage_update_preserves_schedules(self):
        x=self.base(); y=with_voltage(x,replace(x.voltage,revision=5,high10=2450)); self.assertEqual(y.schedules,x.schedules); self.assertEqual(y.schedule_generation,x.schedule_generation)
    def test_schedule_update_preserves_voltage(self):
        x=self.base(); items=[ScheduleItem(0,2,1,0,NONE,600,True)]; y=with_schedules(x,4,items); self.assertEqual(y.voltage,x.voltage); self.assertEqual(y.schedules,tuple(items))
    def test_corrupt_newest_falls_back(self):
        old=self.base(); new=with_voltage(old,replace(old.voltage,revision=5)); a=encode(old); b=bytearray(encode(new)); b[-1]^=0x55
        self.assertEqual(choose(a,b),(SECTOR_A,old))
    def test_every_interrupted_write_keeps_old_or_complete_new(self):
        old=self.base(); new=with_schedules(old,4,[ScheduleItem(0,3,1,0,NONE,700,True)]); a=encode(old); b=encode(new)
        erased=b'\xff'*SECTOR_SIZE
        for cut in range(len(b)+1):
            partial=b[:cut]+erased[cut:]
            sector,img=choose(a,partial)
            self.assertIn(img,(old,new)); self.assertEqual(img,new if cut==len(b) else old)

class FullEngineArbitrationTest(unittest.TestCase):
    def runtime(self,child=0):
        img=EngineImage(voltage=VoltagePolicy(revision=1,enabled=True,protected_mask=0b1111))
        return EngineRuntime(img,child)
    def test_child_lock_blocks_buttons_not_app(self):
        r=self.runtime(31); self.assertFalse(r.button_allowed(0)); self.assertFalse(r.button_allowed(4)); self.assertTrue(r.relay(1,True,'app'))
    def test_voltage_trip_vetoes_all_on_sources_but_allows_off(self):
        r=self.runtime(); r.set_trip(True)
        for src in ('app','schedule','queue','cycle','button'):
            self.assertFalse(r.relay(1,True,src),src)
        r.relays[1]=True; self.assertTrue(r.relay(1,False,'app')); self.assertFalse(r.relays[1])
    def test_unprotected_outlet_can_turn_on_during_trip(self):
        img=EngineImage(voltage=VoltagePolicy(revision=1,enabled=True,protected_mask=0b0001)); r=EngineRuntime(img); r.set_trip(True)
        self.assertFalse(r.relay(1,True,'schedule')); self.assertTrue(r.relay(2,True,'schedule'))

if __name__=='__main__': unittest.main()
