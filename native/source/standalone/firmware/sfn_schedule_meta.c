#include "sfn_schedule_meta.h"
#include <string.h>
void sfn_schedule_meta_default(sfn_schedule_meta *m){if(m)memset(m,0,sizeof(*m));}
static int offset_ok(int x){return x>=-840&&x<=840;}
int sfn_schedule_meta_valid(const sfn_schedule_meta *m){
 if(!m||m->valid>1||m->count>8)return 0;
 if(!m->valid)return m->generation==0&&m->count==0&&m->transition1_epoch==0&&m->transition2_epoch==0&&m->offset_minutes==0&&m->transition1_offset_minutes==0&&m->transition2_offset_minutes==0;
 if(!offset_ok(m->offset_minutes)||!offset_ok(m->transition1_offset_minutes)||!offset_ok(m->transition2_offset_minutes))return 0;
 if(m->transition1_epoch&&m->transition2_epoch&&m->transition2_epoch<=m->transition1_epoch)return 0;
 return 1;
}
int16_t sfn_schedule_offset_at(const sfn_schedule_meta *m,uint32_t epoch){
 int16_t offset;if(!sfn_schedule_meta_valid(m)||!m->valid)return 0;offset=m->offset_minutes;
 if(m->transition1_epoch&&epoch>=m->transition1_epoch)offset=m->transition1_offset_minutes;
 if(m->transition2_epoch&&epoch>=m->transition2_epoch)offset=m->transition2_offset_minutes;
 return offset;
}
