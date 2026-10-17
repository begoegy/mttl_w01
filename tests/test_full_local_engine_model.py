import unittest
from tools.sfn_full_local_engine_model import *

class FullEngineStorageTest(unittest.TestCase):
    def base(self):
        return EngineImage(seq=7,schedule_generation=3,voltage=VoltagePolicy(revision=4,enabled=True,low10=1800,high10=2500,hyst10=50,hold10=20,recovery=30,protected_mask=15,auto_restore=True),schedules=(ScheduleItem(1,1,0,True,NONE,480),ScheduleItem(1,1,0,False,NONE,1320)))
    def test_roundtrip_keeps_voltage_and_schedule_together(self):
        x=self.base(); self.assertEqual(decode(encode(x)),x)
    def test_voltage_update_preserves_schedules(self):
        x=self.base(); y=with_voltage(x,replace(x.voltage,revision=5,high10=2450)); self.assertEqual(y.schedules,x.schedules); self.assertEqual(y.schedule_generation,x.schedule_generation)
    def test_schedule_update_preserves_voltage(self):
        x=self.base(); items=[ScheduleItem(2,1,0,True,NONE,600)]; y=with_schedules(x,4,items); self.assertEqual(y.voltage,x.voltage); self.assertEqual(y.schedules,tuple(items))
    def test_exact_image_size_supports_38_events(self):
        items=tuple(ScheduleItem((i%4)+1,1,0,bool(i%2),NONE,(i*31)%1440) for i in range(MAX_SCHEDULES))
        raw=encode(EngineImage(seq=1,schedule_generation=1,schedules=items))
        self.assertEqual(len(raw),344); self.assertEqual(len(decode(raw).schedules),38)

    def test_corrupt_newest_falls_back(self):
        old=self.base(); new=with_voltage(old,replace(old.voltage,revision=5)); a=encode(old); b=bytearray(encode(new)); used=HEADER.size+len(new.schedules)*ITEM.size; b[used-1]^=0x55
        self.assertEqual(choose(a,b),(SECTOR_A,old))
    def test_every_interrupted_write_keeps_old_or_complete_new(self):
        old=self.base(); new=with_schedules(old,4,[ScheduleItem(3,1,0,True,NONE,700)]); a=encode(old); b=encode(new)
        erased=b'\xff'*IMAGE_SIZE
        for cut in range(len(b)+1):
            partial=b[:cut]+erased[cut:]
            sector,img=choose(a,partial)
            self.assertIn(img,(old,new)); used=HEADER.size+len(new.schedules)*ITEM.size; self.assertEqual(img,new if cut>=used else old)

class FullEngineArbitrationTest(unittest.TestCase):
    def runtime(self,child=0):
        img=EngineImage(seq=1,voltage=VoltagePolicy(revision=1,enabled=True,protected_mask=0b1111))
        return EngineRuntime(img,child)
    def test_child_lock_blocks_buttons_not_app(self):
        r=self.runtime(31); self.assertFalse(r.button_allowed(0)); self.assertFalse(r.button_allowed(4)); self.assertTrue(r.relay(1,True,'app'))
    def test_voltage_trip_vetoes_all_on_sources_but_allows_off(self):
        r=self.runtime(); r.set_trip(True)
        for src in ('app','schedule','queue','cycle','button'):
            self.assertFalse(r.relay(1,True,src),src)
        r.relays[1]=True; self.assertTrue(r.relay(1,False,'app')); self.assertFalse(r.relays[1])
    def test_unprotected_outlet_can_turn_on_during_trip(self):
        img=EngineImage(seq=1,voltage=VoltagePolicy(revision=1,enabled=True,protected_mask=0b0001)); r=EngineRuntime(img); r.set_trip(True)
        self.assertFalse(r.relay(1,True,'schedule')); self.assertTrue(r.relay(2,True,'schedule'))

if __name__=='__main__': unittest.main()
