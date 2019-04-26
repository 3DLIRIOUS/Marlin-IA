#include "Creality_DWIN.h"
#include <HardwareSerial.h>
#include <arduino.h>
#include <wstring.h>
#include <stdio.h>

#include "../ui_api.h"

namespace ExtUI {
const float manual_feedrate_mm_m[] = MANUAL_FEEDRATE;
uint8_t progress_bar_percent;
int startprogress = 0;
CRec CardRecbuf;
int temphot=0;
//int tempbed=0;
//float pause_z = 0;
#if DISABLED(POWER_LOSS_RECOVERY)
	int power_off_type_yes = 0;
	int power_off_commands_count = 0;
#endif

float PLA_ABSModeTemp = 195;
int	last_target_temperature_bed;
int	last_target_temperature[4] = {0};
char waitway = 0;
int recnum = 0;
unsigned char Percentrecord = 0;
float	ChangeMaterialbuf[2] = {0};

char NozzleTempStatus[3] = {0};


bool PrintMode = true;

char PrintStatue[2] = {0};	//PrintStatue[0], 0 represent  to 43 page, 1 represent to 44 page
bool CardUpdate = false;	//represents to update file list
char CardCheckStatus[2] = {0};	//CardCheckStatus[0] represents to check card in printing and after making sure to begin and to check card in heating with value as 1, but 0 for off
unsigned char LanguageRecbuf; // !0 represent Chinese, 0 represent English
char PrinterStatusKey[2] = {0};	// PrinterStatusKey[1] value: 0 represents to keep temperature, 1 represents  to heating , 2 stands for cooling , 3 stands for printing
							// PrinterStatusKey[0] value: 0 reprensents 3D printer ready
bool lcd_sd_status;//represents SD-card status, true means SD is available, false means opposite.

char Checkfilenum=0;
char FilenamesCount = 0;
char cmdbuf[20] = {0};
char FilementStatus[2] = {0};

unsigned char	AxisPagenum = 0;	//0 for 10mm, 1 for 1mm, 2 for 0.1mm
bool InforShowStatus = true;
bool TPShowStatus = false;	// true for only opening time and percentage, false for closing time and percentage.
bool FanStatus = true;
bool AutohomeKey = false;
unsigned char AutoHomeIconNum;
unsigned long VolumeSet = 0x80;
extern char power_off_commands[9][96];
bool PoweroffContinue = false;
extern const char *injected_commands_P;
char commandbuf[30];

  void onStartup() {
		Serial2.begin(115200);
		LanguageRecbuf = 0; //Force language to English, 1=Chinese but currently not implemented
		int showcount = 0;

		rtscheck.recdat.head[0] = rtscheck.snddat.head[0] = FHONE;
		rtscheck.recdat.head[1] = rtscheck.snddat.head[1] = FHTWO;
		memset(rtscheck.databuf,0, sizeof(rtscheck.databuf));

		#if HAS_MESH && (ENABLED(MachineCR10SPro) || ENABLED(Force10SProDisplay))
			if (ExtUI::getMeshValid())
			{
				//bed_mesh_t bedMesh = ExtUI::getMeshArray();
				for(int xCount  = 0; xCount < GRID_MAX_POINTS_X; xCount++)
				{
					for(int yCount  = 0; yCount < GRID_MAX_POINTS_X; yCount++)
					{
						if((showcount++) < 16)
						{
							rtscheck.RTS_SndData(getMeshArray[xCount][yCount] *10000, AutolevelVal + (15-showcount-1)*2);
							rtscheck.RTS_SndData(showcount,AutolevelIcon);
						}
					}
				}
				rtscheck.RTS_SndData(2, AutoLevelIcon); //On
				enqueueCommands_P((PSTR("M420 S1"))); // Enable Bed leveling if mesh found and valid
			}
			else 
			{
				rtscheck.RTS_SndData(3, AutoLevelIcon); //Off
			}
		#endif
	
		//VolumeSet = eeprom_read_byte((unsigned char*)FONT_EEPROM+4);
		//if(VolumeSet < 0 || VolumeSet > 0xFF)
			VolumeSet = 0x20;
			
		if(PrintMode)
			rtscheck.RTS_SndData(3, FanKeyIcon+1);	// saving mode
		else 
			rtscheck.RTS_SndData(2, FanKeyIcon+1);	// normal
		last_target_temperature_bed = getTargetTemp_celsius(BED); 
		last_target_temperature[0] =  getTargetTemp_celsius(H0);
		rtscheck.RTS_SndData(100,FeedrateDisplay);
		
		/***************turn off motor*****************/
		rtscheck.RTS_SndData(11, FilenameIcon); 
		
		/***************transmit temperature to screen*****************/
		rtscheck.RTS_SndData(0, NozzlePreheat);
		rtscheck.RTS_SndData(0, BedPreheat);
		rtscheck.RTS_SndData(getActualTemp_celsius(H0), NozzleTemp);
		rtscheck.RTS_SndData(getActualTemp_celsius(BED), Bedtemp);
		/***************transmit Fan speed to screen*****************/
		rtscheck.RTS_SndData(2, FanKeyIcon);	//turn 0ff fan icon
		FanStatus = true;
		
		/***************transmit Printer information to screen*****************/
		for(int j = 0;j < 20;j++)	//clean filename
			rtscheck.RTS_SndData(0,MacVersion+j);
		char sizebuf[20]={0};
		sprintf(sizebuf,"%d X %d X %d",Y_BED_SIZE, X_BED_SIZE, Z_MAX_POS);
		rtscheck.RTS_SndData(CUSTOM_MACHINE_NAME, MacVersion);
		rtscheck.RTS_SndData(DETAILED_BUILD_VERSION, SoftVersion);
		rtscheck.RTS_SndData(sizebuf, PrinterSize);
		rtscheck.RTS_SndData(WEBSITE_URL, CorpWebsite);

		/**************************some info init*******************************/
		rtscheck.RTS_SndData(0,PrintscheduleIcon);
		rtscheck.RTS_SndData(0,PrintscheduleIcon+1);

		/************************clean screen*******************************/
		for(int i = 0;i < MaxFileNumber;i++)
		{
			for(int j = 0;j < 10;j++)
				rtscheck.RTS_SndData(0,SDFILE_ADDR +i*10+j);
		}
		
		for(int j = 0;j < 10;j++)	
		{
			rtscheck.RTS_SndData(0,Printfilename+j); //clean screen.
			rtscheck.RTS_SndData(0,Choosefilename+j); //clean filename
		}
		for(int j = 0;j < 8;j++)
			rtscheck.RTS_SndData(0,FilenameCount+j);
		for(int j = 1;j <= MaxFileNumber;j++)
		{
			rtscheck.RTS_SndData(10,FilenameIcon+j);
			rtscheck.RTS_SndData(10,FilenameIcon1+j);
		}
		
		SERIAL_ECHOLNPAIR("\n init zprobe_zoffset = ",getZOffset_mm());
		rtscheck.RTS_SndData(getZOffset_mm()*100, 0x1026);
		
		SERIAL_ECHOLN("==Dwin Init Complete==");
  }

  void onIdle() {

		// After homing, reset back to motion screen
	 if(isAxisPositionKnown((axis_t)X) && !isAxisPositionKnown((axis_t)Y) && !isAxisPositionKnown((axis_t)Z)) {

		 switch(waitway)
		 {
			 case 1 : 
				InforShowStatus = true;
				rtscheck.RTS_SndData(4+CEIconGrap,IconPrintstatus);	// 4 for Pause
				rtscheck.RTS_SndData(ExchangePageBase + 54, ExchangepageAddr); 
				waitway = 0;
			 break;

			 case 2: 
				waitway = 0;
			 break;

			 case 3:
				waitway = 0;
				rtscheck.RTS_SndData(ExchangePageBase + 64, ExchangepageAddr);
			 break;

			case 4:
				if(AutohomeKey) { //Manual Move Home Done
					rtscheck.RTS_SndData(ExchangePageBase + 71 + AxisPagenum, ExchangepageAddr);
					AutohomeKey = false;
					waitway = 0;
				}
			break;
			 case 5:
				InforShowStatus = true;
				waitway = 0;
				rtscheck.RTS_SndData(ExchangePageBase + 78, ExchangepageAddr); //exchange to 78 page
				break;
		 }

		#if ENABLED(POWER_LOSS_RECOVERY)
			if(PoweroffContinue)
			{
				PoweroffContinue = false;
				enqueue_and_echo_command(power_off_commands[3]);
				card.startFileprint();
				print_job_timer.power_off_start();
			}
		#endif
	}

	if(InforShowStatus)
	{
		if ((power_off_type_yes == 0)  && lcd_sd_status && (power_off_commands_count > 0)) // print the file before the power is off.
		{
			SERIAL_ECHOLN("  ***test1*** ");
			if(startprogress == 0)
			{
				rtscheck.RTS_SndData(StartSoundSet, SoundAddr);
				
				rtscheck.RTS_SndData(5, VolumeIcon);
				rtscheck.RTS_SndData(8, SoundIcon);
				rtscheck.RTS_SndData(0xC0, VolumeIcon-2);
				if(VolumeSet == 0)
				{
					rtscheck.RTS_SndData(0, VolumeIcon);
					rtscheck.RTS_SndData(9, SoundIcon);
				}
				else
				{
					rtscheck.RTS_SndData((VolumeSet+1)/32 - 1, VolumeIcon);
					rtscheck.RTS_SndData(8, SoundIcon);
				}
				rtscheck.RTS_SndData(VolumeSet, VolumeIcon-2);
				rtscheck.RTS_SndData(VolumeSet<<8, SoundAddr+1);
			}
			if(startprogress <= 100)
				rtscheck.RTS_SndData(startprogress,StartIcon);
			else
				rtscheck.RTS_SndData((startprogress-100),StartIcon+1);
			delay(30);
			if((startprogress +=1) > 200)
			{
						#if ENABLED(POWER_LOSS_RECOVERY)
				      		power_off_type_yes = 1;
						
					for (uint16_t i = 0; i < CardRecbuf.Filesum ; i++) 
					{
						if(!strcmp(CardRecbuf.Cardfilename[i], &power_off_info.sd_filename[1]))
						{
							InforShowStatus = true;
							int filelen = strlen(CardRecbuf.Cardshowfilename[i]);
							filelen = (TEXTBYTELEN - filelen)/2;
							if(filelen > 0)
							{
								char buf[20];
								memset(buf,0,sizeof(buf));
								strncpy(buf,"         ",filelen);
								strcpy(&buf[filelen],CardRecbuf.Cardshowfilename[i]);
								RTS_SndData(buf, Printfilename);
							}
							else
								RTS_SndData(CardRecbuf.Cardshowfilename[i],Printfilename); //filenames
								RTS_SndData(ExchangePageBase + 76, ExchangepageAddr); 
							break;
						}
					}
					#endif
			}
			
			return;

		}	
		else if((power_off_type_yes == 0) && !power_off_commands_count )
		{
			
			if(startprogress == 0)
			{
				rtscheck.RTS_SndData(StartSoundSet, SoundAddr);
				
				if(VolumeSet == 0)
				{
					rtscheck.RTS_SndData(0, VolumeIcon);
					rtscheck.RTS_SndData(9, SoundIcon);
				}
				else
				{
					rtscheck.RTS_SndData((VolumeSet+1)/32 - 1, VolumeIcon);
					rtscheck.RTS_SndData(8, SoundIcon);
				}
				rtscheck.RTS_SndData(VolumeSet, VolumeIcon-2);
				rtscheck.RTS_SndData(VolumeSet<<8, SoundAddr+1);
			}
			if(startprogress <= 100)
				rtscheck.RTS_SndData(startprogress,StartIcon);
			else
				rtscheck.RTS_SndData((startprogress-100),StartIcon+1);
			delay(30);
			if((startprogress +=1) > 200)
			{
				SERIAL_ECHOLN("  startprogress ");
			   	power_off_type_yes = 1;
				InforShowStatus = true;
				TPShowStatus = false;
				rtscheck.RTS_SndData(ExchangePageBase + 45, ExchangepageAddr); 
			}
			return;
		}
		else
		{
			if (TPShowStatus && isPrinting())		//need to optimize
			{
				static unsigned int last_cardpercentValue = 101; 
				rtscheck.RTS_SndData(getProgress_seconds_elapsed() / 3600,Timehour);		
				rtscheck.RTS_SndData((getProgress_seconds_elapsed() /3600)/60,Timemin);	
				
				if(last_cardpercentValue != getProgress_percent())
				{
					if( progress_bar_percent > 0 )
					{	
						Percentrecord = progress_bar_percent+1;
						if(Percentrecord<= 50)
						{
							rtscheck.RTS_SndData((unsigned int)Percentrecord*2 ,PrintscheduleIcon);
							rtscheck.RTS_SndData(0,PrintscheduleIcon+1);
						}
						else
						{
							rtscheck.RTS_SndData(100 ,PrintscheduleIcon);
							rtscheck.RTS_SndData((unsigned int)Percentrecord*2 -100,PrintscheduleIcon+1);
						}
					}	
					else
					{
						rtscheck.RTS_SndData(0,PrintscheduleIcon);
						rtscheck.RTS_SndData(0,PrintscheduleIcon+1);
					}
					rtscheck.RTS_SndData((unsigned int)getProgress_percent(),Percentage);
					last_cardpercentValue = getProgress_percent();
				}
			}
			
			rtscheck.RTS_SndData(getZOffset_mm()*100, 0x1026); 
			//float temp_buf = getActualTemp_celsius(H0);
			rtscheck.RTS_SndData(getActualTemp_celsius(H0),NozzleTemp);
			rtscheck.RTS_SndData(getActualTemp_celsius(BED),Bedtemp);
			if(last_target_temperature_bed != getTargetTemp_celsius(BED) || (last_target_temperature[0] != getTargetTemp_celsius(H0)))
			{
				rtscheck.RTS_SndData(getTargetTemp_celsius(H0),NozzlePreheat);
				rtscheck.RTS_SndData(getTargetTemp_celsius(BED),BedPreheat);

				if(isPrinting())
				{
					//keep the icon
				}
				else if(last_target_temperature_bed < getTargetTemp_celsius(BED) || (last_target_temperature[0] < getTargetTemp_celsius(H0)))
				{
					rtscheck.RTS_SndData(1+CEIconGrap,IconPrintstatus);
					PrinterStatusKey[1] =( PrinterStatusKey[1] == 0? 1 : PrinterStatusKey[1]);
				}
				else if(last_target_temperature_bed > getTargetTemp_celsius(BED) || (last_target_temperature[0] > getTargetTemp_celsius(H0)))
				{
					rtscheck.RTS_SndData(8+CEIconGrap,IconPrintstatus);
					PrinterStatusKey[1] =( PrinterStatusKey[1] == 0? 2 : PrinterStatusKey[1] );
				}
					
				last_target_temperature_bed = getTargetTemp_celsius(BED);
				last_target_temperature[0] = getTargetTemp_celsius(H0);

			}

			if(NozzleTempStatus[0] || NozzleTempStatus[2])	//statuse of loadfilement and unloadfinement when temperature is less than
			{
				unsigned int IconTemp;
				
				IconTemp = getActualTemp_celsius(H0) * 100/getTargetTemp_celsius(H0);
				if(IconTemp >= 100)
					IconTemp = 100;
				rtscheck.RTS_SndData(IconTemp, HeatPercentIcon);
				if(getActualTemp_celsius(H0) >= getTargetTemp_celsius(H0) && NozzleTempStatus[0])
				{
					NozzleTempStatus[1] = 0;
					NozzleTempStatus[0] = 0;
					rtscheck.RTS_SndData(10*ChangeMaterialbuf[0], FilementUnit1);	
					rtscheck.RTS_SndData(10*ChangeMaterialbuf[1], FilementUnit2);
					rtscheck.RTS_SndData(ExchangePageBase + 65, ExchangepageAddr); 
					//RTS_line_to_current(E_AXIS); //NEEDS FIX
					setActiveTool(E0, true);
					//delay(current_position[E_AXIS] * 1000);
				}
				else if(getActualTemp_celsius(H0) >= getTargetTemp_celsius(H0) && NozzleTempStatus[2])
				{
					SERIAL_ECHOPAIR("\n ***NozzleTempStatus[2] =",(int)NozzleTempStatus[2]);
					startprogress = NozzleTempStatus[2] = 0;
					TPShowStatus = true;
					rtscheck.RTS_SndData(4, ExchFlmntIcon);
					rtscheck.RTS_SndData(ExchangePageBase + 83, ExchangepageAddr); 
				}
				else if( NozzleTempStatus[2] )
				{
					rtscheck.RTS_SndData((startprogress++)%5, ExchFlmntIcon);
				}
			}
			if(AutohomeKey )
			{
				rtscheck.RTS_SndData(AutoHomeIconNum++,AutoZeroIcon);
				if(AutoHomeIconNum > 9)	AutoHomeIconNum = 0;
			}

			rtscheck.RTS_SndData(10*getAxisPosition_mm((axis_t)X), DisplayXaxis);
			rtscheck.RTS_SndData(10*getAxisPosition_mm((axis_t)Y), DisplayYaxis);
			rtscheck.RTS_SndData(10*getAxisPosition_mm((axis_t)Z), DisplayZaxis);
		}

		if(getLevelingActive()) 
			rtscheck.RTS_SndData(2, AutoLevelIcon);/*Off*/
		else
			rtscheck.RTS_SndData(3, AutoLevelIcon);/*On*/
	}
	if(rtscheck.RTS_RecData() > 0)
		//SERIAL_PROTOCOLLN("  Handle Data ");
	    rtscheck.RTS_HandleData();

}


RTSSHOW::RTSSHOW(){
  recdat.head[0] = snddat.head[0] = FHONE;
  recdat.head[1] = snddat.head[1] = FHTWO;
  memset(databuf,0, sizeof(databuf));
}

int RTSSHOW::RTS_RecData()
{
  while(Serial2.available() > 0 && (recnum < SizeofDatabuf))
  {
    databuf[recnum] = Serial2.read();
    if(databuf[0] != FHONE)    //ignore the invalid data
    {
    	 if(recnum > 0) // prevent the program from running.
    	 {
	    	 memset(databuf,0,sizeof(databuf));
		 recnum = 0;
    	 }
        continue;
    }
    delay(10);
    recnum++;
  }

  if(recnum < 1)    //receive nothing  	
    return -1;
  else  if((recdat.head[0] == databuf[0]) && (recdat.head[1] == databuf[1]) && recnum > 2)
  {
  	//  SERIAL_ECHOLN(" *** RTS_RecData1*** ");

    	recdat.len = databuf[2];
    	recdat.command = databuf[3];
	if(recdat.len == 0x03 && (recdat.command == 0x82 || recdat.command == 0x80) && (databuf[4] == 0x4F) && (databuf[5] == 0x4B))  //response for writing byte
	{   
            memset(databuf,0, sizeof(databuf));
            recnum = 0;
	     //SERIAL_ECHOLN(" *** RTS_RecData1*** ");
            return -1;
	}
	else if(recdat.command == 0x83) //response for reading the data from the variate
	{
            recdat.addr = databuf[4];
            recdat.addr = (recdat.addr << 8 ) | databuf[5];
            recdat.bytelen = databuf[6];
	     for(int i = 0;i < recdat.bytelen;i+=2)
	     {
			recdat.data[i/2]= databuf[7+i];
			recdat.data[i/2]= (recdat.data[i/2] << 8 )| databuf[8+i];
	     }
	}
	else if(recdat.command == 0x81)  //response for reading the page from the register
	{
            recdat.addr = databuf[4];
            recdat.bytelen = databuf[5];
	     for(int i = 0;i < recdat.bytelen;i++)
	     {
			recdat.data[i]= databuf[6+i];
	            //recdat.data[i]= (recdat.data[i] << 8 )| databuf[7+i];
	     }
       }
  
  }
  else
  {
	memset(databuf,0, sizeof(databuf));
	recnum = 0;
       return -1;  //receive the wrong data
  }
    memset(databuf,0, sizeof(databuf));
    recnum = 0;
    return 2;
}

void RTSSHOW::RTS_SndData(void)
{
    if((snddat.head[0] == FHONE) && (snddat.head[1] == FHTWO) && snddat.len >= 3){
          databuf[0] = snddat.head[0];
          databuf[1] = snddat.head[1];
          databuf[2] = snddat.len;
          databuf[3] = snddat.command;
          if(snddat.command ==0x80)    //to write data to the register
          {
		databuf[4] = snddat.addr;
		for(int i =0;i <(snddat.len - 2);i++)
			databuf[5+i] = snddat.data[i];
          }
          else if(snddat.len == 3 && (snddat.command ==0x81))   //to read data from the register
          {
                databuf[4] = snddat.addr;
                databuf[5] = snddat.bytelen;
          }
          else if(snddat.command ==0x82)   //to write data to the variate
          {
                databuf[4] = snddat.addr >> 8;
                databuf[5] = snddat.addr & 0xFF;
		   for(int i =0;i <(snddat.len - 3);i += 2)
		   {
			databuf[6 + i] = snddat.data[i/2] >> 8;
			databuf[7 + i] = snddat.data[i/2] & 0xFF;
		   }
          }
          else if(snddat.len == 4 && (snddat.command ==0x83))   //to read data from the variate
          {
                databuf[4] = snddat.addr >> 8;
                databuf[5] = snddat.addr & 0xFF;
                databuf[6] = snddat.bytelen;
          }
            for(int i = 0;i < (snddat.len + 3);i++)
            {
                Serial2.write(databuf[i]);
                delayMicroseconds(1);
            }
			
          memset(&snddat,0,sizeof(snddat));
          memset(databuf,0, sizeof(databuf));
          snddat.head[0] = FHONE;
          snddat.head[1] = FHTWO;
    }
}


void RTSSHOW::RTS_SndData(const String &s, unsigned long addr, unsigned char cmd /*= VarAddr_W*/)
{
	if(s.length() < 1)
		return;
	RTS_SndData(s.c_str(), addr, cmd);
}

void RTSSHOW::RTS_SndData(const char *str, unsigned long addr, unsigned char cmd/*= VarAddr_W*/)
{

	int len = strlen(str);
	
	if( len > 0)
	{
		databuf[0] = FHONE;
		databuf[1] = FHTWO;
		databuf[2] = 3+len;
		databuf[3] = cmd;
		databuf[4] = addr >> 8;
		databuf[5] = addr & 0x00FF;
		for(int i =0;i <len ;i++)
			databuf[6 + i] = str[i];

		for(int i = 0;i < (len + 6);i++)
		{
		    Serial2.write(databuf[i]);
		    delayMicroseconds(1);
		}
	      memset(databuf,0, sizeof(databuf));
	}
}

void RTSSHOW::RTS_SndData(char c, unsigned long addr, unsigned char cmd/*= VarAddr_W*/)
{
	snddat.command = cmd;
	snddat.addr = addr;
	snddat.data[0] = (unsigned long)c;
	snddat.data[0] = snddat.data[0] << 8;
	snddat.len = 5;
	RTS_SndData();
}

void RTSSHOW::RTS_SndData(unsigned char* str, unsigned long addr, unsigned char cmd){RTS_SndData((char *)str, addr, cmd);}

void RTSSHOW::RTS_SndData(int n, unsigned long addr, unsigned char cmd/*= VarAddr_W*/)
{
	if(cmd == VarAddr_W )
	{
		if(n > 0xFFFF)
		{
			snddat.data[0] = n >> 16;
			snddat.data[1] = n & 0xFFFF;
			snddat.len = 7;
		}
		else
		{
			snddat.data[0] = n;
			snddat.len = 5;
		}
	}
	else if(cmd == RegAddr_W)
	{
		snddat.data[0] = n;
		snddat.len = 3;
	}
	else if(cmd == VarAddr_R)
	{
		snddat.bytelen = n;
		snddat.len = 4;
	}
	snddat.command = cmd;
	snddat.addr = addr;
	RTS_SndData();
}

void RTSSHOW::RTS_SndData(unsigned int n, unsigned long addr, unsigned char cmd){ RTS_SndData((int)n, addr, cmd); }

void RTSSHOW::RTS_SndData(float n, unsigned long addr, unsigned char cmd){ RTS_SndData((int)n, addr, cmd); }

void RTSSHOW::RTS_SndData(long n, unsigned long addr, unsigned char cmd){ RTS_SndData((unsigned long)n, addr, cmd); }

void RTSSHOW::RTS_SndData(unsigned long n, unsigned long addr, unsigned char cmd/*= VarAddr_W*/)
{
	if(cmd == VarAddr_W )
	{
		if(n > 0xFFFF)
		{
			snddat.data[0] = n >> 16;
			snddat.data[1] = n & 0xFFFF;
			snddat.len = 7;
		}
		else
		{
			snddat.data[0] = n;
			snddat.len = 5;
		}
	}
	else if(cmd == VarAddr_R)
	{
		snddat.bytelen = n;
		snddat.len = 4;
	}
	snddat.command = cmd;
	snddat.addr = addr;
	RTS_SndData();
}


void RTSSHOW::RTS_HandleData()
{
	int Checkkey = -1;
	SERIAL_ECHOLN("  *******RTS_HandleData******** ");
	if(waitway > 0)	//for waiting
	{
		SERIAL_ECHOPAIR("waitway ==", (int)waitway);
		memset(&recdat,0 , sizeof(recdat));
		recdat.head[0] = FHONE;
		recdat.head[1] = FHTWO;
		return;
	}
	SERIAL_ECHOPAIR("recdat.data[0] ==", recdat.data[0]);
	SERIAL_ECHOPAIR("recdat.addr ==", recdat.addr);
    for(int i = 0;Addrbuf[i] != 0;i++)
    {
	  if(recdat.addr == Addrbuf[i])
	  {
	  	if(Addrbuf[i] >= Stopprint && Addrbuf[i] <= Resumeprint)
			Checkkey = PrintChoice;
		else if(Addrbuf[i] == NzBdSet || Addrbuf[i] == NozzlePreheat || Addrbuf[i] == BedPreheat )
			Checkkey = ManualSetTemp;
		else if(Addrbuf[i] >= AutoZero && Addrbuf[i] <= DisplayZaxis)
			Checkkey = XYZEaxis;
		else if(Addrbuf[i] >= FilementUnit1 && Addrbuf[i] <= FilementUnit2)
			Checkkey = Filement;
		else
			Checkkey = i;
		break;
	  }
    }
	
    if(recdat.addr >= SDFILE_ADDR && recdat.addr <= (SDFILE_ADDR + 10 *(FileNum+1)))
	   Checkkey = Filename;
  
    if(Checkkey < 0)
    {
    	memset(&recdat,0 , sizeof(recdat));
	recdat.head[0] = FHONE;
	recdat.head[1] = FHTWO;
	return;
    }
SERIAL_ECHO("== Checkkey==");
SERIAL_ECHO(Checkkey);
    switch(Checkkey)
    {
	case Printfile :
		if(recdat.data[0] == 1)		// card
		{
			InforShowStatus = false;
			CardUpdate = true;
			CardRecbuf.recordcount = -1;
			//RTS_SDCardUpate(); //FIX ME
			SERIAL_ECHO("\n Handle Data PrintFile 1 Setting Screen ");
			RTS_SndData(ExchangePageBase + 46, ExchangepageAddr); 

		}
		else if(recdat.data[0] == 2)	// return after printing result.
		{
			InforShowStatus = true;
		      TPShowStatus = false;
			stopPrint();
			enqueueCommands_P(PSTR("M84"));
			RTS_SndData(11, FilenameIcon); 
			RTS_SndData(0,PrintscheduleIcon);
			RTS_SndData(0,PrintscheduleIcon+1);
			RTS_SndData(0,Percentage);
			delay(2);
			RTS_SndData(0,Timehour);
			RTS_SndData(0,Timemin);
			
			SERIAL_ECHO("\n Handle Data PrintFile 2 Setting Screen ");
			RTS_SndData(ExchangePageBase + 45, ExchangepageAddr); //exchange to 45 page
		} 
		else if(recdat.data[0] == 3 )	// Temperature control
		{
			InforShowStatus = true;
		      TPShowStatus = false;
			  SERIAL_ECHO("\n Handle Data PrintFile 3 Setting Screen ");
				if(FanStatus)
					RTS_SndData(ExchangePageBase + 58, ExchangepageAddr); //exchange to 58 page, the fans off
				else
					RTS_SndData(ExchangePageBase + 57, ExchangepageAddr); //exchange to 57 page, the fans on
		}
		else if(recdat.data[0] == 4 )	//Settings
			InforShowStatus = false;
		break;
		
	case Ajust :
		if(recdat.data[0] == 1)
		{
			InforShowStatus = false;
			FanStatus?RTS_SndData(2, FanKeyIcon):RTS_SndData(3, FanKeyIcon);
		}
		else if(recdat.data[0] == 2)
		{
			SERIAL_ECHO("\n Handle Data Adjust 2 Setting Screen ");
			InforShowStatus = true;
			if(PrinterStatusKey[1] == 3)// during heating
			{
				RTS_SndData(ExchangePageBase + 52, ExchangepageAddr); 
			}
			else if(PrinterStatusKey[1] == 4)
			{
				RTS_SndData(ExchangePageBase + 54, ExchangepageAddr); 
			}
			else
			{
				RTS_SndData(ExchangePageBase + 53, ExchangepageAddr); 

			}
		}
		else if(recdat.data[0] == 3)
		{
			if(FanStatus)	//turn on the fan
			{
				RTS_SndData(3, FanKeyIcon); 
				setTargetFan_percent(100, FAN0);
				FanStatus = false;
			}
			else//turn off the fan
			{
				RTS_SndData(2, FanKeyIcon); 
				setTargetFan_percent(0, FAN0);
				FanStatus = true;
			}
		}
		else if(recdat.data[0] == 4)
		{
			if(PrintMode)	// normal printing mode
			{
				RTS_SndData(2, FanKeyIcon+1); 
				PrintMode = false;
			}
			else // power saving mode
			{
				RTS_SndData(3, FanKeyIcon+1); 
  				PrintMode = true;
			}

		}
		break;
		
	case Feedrate :
		setFeedrate_percent(recdat.data[0]);
		break;

	case PrintChoice:
		if(recdat.addr == Stopprint)
		{ 
			if(PrintStatue[1] == 1 && recdat.data[0] == 0xF0)	// in the pause
			{
				RTS_SndData(ExchangePageBase + 54, ExchangepageAddr); 
				break;
			}
			else if(PrintStatue[1] == 0 && recdat.data[0] == 0xF0)	
			{
				if(PrinterStatusKey[1] == 3)// during heating
				{
					RTS_SndData(ExchangePageBase + 52, ExchangepageAddr); 
				}
				else
				{
					RTS_SndData(ExchangePageBase + 53, ExchangepageAddr); 
				}
				break;
			}
			//FilementStatus[0]  =  0; // recover the status waiting to check filements
			

			RTS_SndData(ExchangePageBase + 87, ExchangepageAddr); 
			RTS_SndData(0,Timehour);		
			RTS_SndData(0,Timemin);	
			CardCheckStatus[0] = 0;// close the key of  checking card in  printing
			//RTS_SDcard_Stop(); //FIX ME
		}
		else if(recdat.addr == Pauseprint)
		{				
			if(recdat.data[0] != 0xF1)
				break;

			RTS_SndData(ExchangePageBase + 87, ExchangepageAddr); 
			pausePrint();
		}
		else if(recdat.addr == Resumeprint && recdat.data[0] == 1)
		{				
			#if ENABLED(MachineCR10SPro) || ENABLED(AddonFilSensor)
			/**************checking filement status during printing************/
			/* //FIX ME
			if(RTS_CheckFilement(0)) 
			{
				for(startprogress=0;startprogress < 5;startprogress++)
				{
					RTS_SndData(startprogress, ExchFlmntIcon);
					delay(400);
				}
				break;
			} */
			#endif
			
			resumePrint();
			FilementStatus[1] = 2;
			
			PrinterStatusKey[1] = 0;
			InforShowStatus = true;

			RTS_SndData(0+CEIconGrap,IconPrintstatus);
			PrintStatue[1] = 0;
			//PrinterStatusKey[1] = 3;
			CardCheckStatus[0] = 1;	// open the key of  checking card in  printing
			RTS_SndData(ExchangePageBase + 52, ExchangepageAddr); 
		}
		if(recdat.addr == Resumeprint && recdat.data[0] == 2)	// warming
		{ 
			NozzleTempStatus[2] = 1;
			PrinterStatusKey[1] = 0;
			InforShowStatus = true;
			setTargetTemp_celsius((float)temphot, H0);
			startprogress  = 0;
			FilementStatus[1] = 2;
			RTS_SndData(ExchangePageBase + 82, ExchangepageAddr); 
		}
		break;
		
	case Zoffset:
		//SERIAL_ECHOPAIR("\n rts_probe_zoffset = ",rts_probe_zoffset);
		//SERIAL_ECHOPAIR("\n rcv data = ",recdat.data[0]);
		float tmp_zprobe_offset;
		if(recdat.data[0]>= 32768) {
			tmp_zprobe_offset = ((float)recdat.data[0]-65536)/100;
		}
		else {
			tmp_zprobe_offset = ((float)recdat.data[0])/100;
		}
		
		//SERIAL_ECHOPAIR("\n rts_probe_zoffset = ",rts_probe_zoffset);
		//SERIAL_ECHOPAIR("\n target = ",(zprobe_zoffset - rts_probe_zoffset));
		//SERIAL_ECHOPAIR("\n target axis = ",Z_AXIS);
		//SERIAL_ECHOPAIR("\n steps mm = ",planner.steps_to_mm[Z_AXIS]);
        if (WITHIN((tmp_zprobe_offset), Z_PROBE_OFFSET_RANGE_MIN, Z_PROBE_OFFSET_RANGE_MAX)) {
        	babystepAxis_steps((400 * (getZOffset_mm() - tmp_zprobe_offset) * -1), (axis_t)Z);
        	setZOffset_mm(tmp_zprobe_offset);
			//SERIAL_ECHOPAIR("\n StepsMoved = ",(400 * (zprobe_zoffset - rts_probe_zoffset) * -1));
			//SERIAL_ECHOPAIR("\n probe_zoffset = ",zprobe_zoffset);
			RTS_SndData(getZOffset_mm()*100, 0x1026);  
		}
		//SERIAL_ECHOPAIR("\n rts_probe_zoffset = ",rts_probe_zoffset);
		enqueueCommands_P((PSTR("M500")));
		//SERIAL_ECHOPAIR("\n probe_zoffset = ",zprobe_zoffset);
		break;
		
	case TempControl:
		if(recdat.data[0] == 0)
		{
			InforShowStatus = true;
			TPShowStatus = false;
		}
		else if(recdat.data[0] == 1)
		{
			/*
				if(FanStatus)
					RTS_SndData(ExchangePageBase + 60, ExchangepageAddr); //exchange to 60 page, the fans off
				else
					RTS_SndData(ExchangePageBase + 59, ExchangepageAddr); //exchange to 59 page, the fans on
			 */
		}
		else if(recdat.data[0] == 2)
		{
			//InforShowStatus = true;
		}
		else if(recdat.data[0] == 3)
		{
			if(FanStatus)	//turn on the fan
			{
				setTargetFan_percent(100, FAN0);
				FanStatus = false;
				RTS_SndData(ExchangePageBase + 57, ExchangepageAddr); //exchange to 57 page, the fans on
			}
			else//turn off the fan
			{
				setTargetFan_percent(0, FAN0);
				FanStatus = true;
				RTS_SndData(ExchangePageBase + 58, ExchangepageAddr); //exchange to 58 page, the fans on
			}
		}
		else if(recdat.data[0] == 5)	//PLA mode
		{
			setTargetTemp_celsius((PLA_ABSModeTemp = PREHEAT_1_TEMP_HOTEND), H0);
			setTargetTemp_celsius(PREHEAT_1_TEMP_BED, BED);

			RTS_SndData(PREHEAT_1_TEMP_HOTEND,NozzlePreheat);
			RTS_SndData(PREHEAT_1_TEMP_BED,BedPreheat);

		}
		else if(recdat.data[0] == 6)	//ABS mode
		{
			setTargetTemp_celsius((PLA_ABSModeTemp = PREHEAT_2_TEMP_HOTEND), H0);
			setTargetTemp_celsius(PREHEAT_2_TEMP_BED, BED);
			
			RTS_SndData(PREHEAT_2_TEMP_HOTEND,NozzlePreheat);
			RTS_SndData(PREHEAT_2_TEMP_BED,BedPreheat);
		}
		else if(recdat.data[0] == 0xF1)
		{
			//InforShowStatus = true;
			#if FAN_COUNT > 0
			for (uint8_t i = 0; i < FAN_COUNT; i++) setTargetFan_percent(0, (fan_t)i);
			#endif
			FanStatus = false;
			setTargetTemp_celsius(0.0, H0);
			setTargetTemp_celsius(0.0, BED);

			RTS_SndData(0,NozzlePreheat);
			delay(1);
			RTS_SndData(0,BedPreheat);
			delay(1);
			
			RTS_SndData(8+CEIconGrap,IconPrintstatus);
			RTS_SndData(ExchangePageBase + 57, ExchangepageAddr);
			PrinterStatusKey[1] = 2;
		}
		break;

	case ManualSetTemp:
		if(recdat.addr == NzBdSet)
		{
			if(recdat.data[0] == 0)
			{
					if(FanStatus)
						RTS_SndData(ExchangePageBase + 58, ExchangepageAddr); //exchange to 58 page, the fans off
					else
						RTS_SndData(ExchangePageBase + 57, ExchangepageAddr); //exchange to 57 page, the fans on
			}
			else if(recdat.data[0] == 1)
			{
				setTargetTemp_celsius(0.0, H0);
				RTS_SndData(0,NozzlePreheat);
			}
			else if(recdat.data[0] == 2)
			{
				setTargetTemp_celsius(0.0, BED);
				RTS_SndData(0,BedPreheat);
			}
		}
		else if(recdat.addr == NozzlePreheat)
		{
			setTargetTemp_celsius((float)recdat.data[0], H0);
		}
		else if(recdat.addr == BedPreheat)
		{
			setTargetTemp_celsius((float)recdat.data[0], BED);
		}
		break;

	case Setting:
		if(recdat.data[0] == 0)	// return to main page
		{
			InforShowStatus = true;
			TPShowStatus = false;
		}
		else if(recdat.data[0] == 1)	//Bed Autoleveling
		{
			if (getLevelingActive())
				RTS_SndData(2, AutoLevelIcon);
			else
				RTS_SndData(3, AutoLevelIcon);
				
			RTS_SndData(10, FilenameIcon);	//Motor Icon
			waitway = 2;		//only for prohibiting to receive massage
			if (!isPositionKnown())
				enqueueCommands_P((PSTR("G28")));
			waitway = 2;
			enqueueCommands_P((PSTR("G1 F100 Z0.0")));
			RTS_SndData(ExchangePageBase + 64, ExchangepageAddr); 
		}
		else if(recdat.data[0] == 2)	// Exchange filement
		{
			InforShowStatus = true;
			TPShowStatus = false;
			memset(ChangeMaterialbuf,0,sizeof(ChangeMaterialbuf));
			ChangeMaterialbuf[1]=ChangeMaterialbuf[0] = 10;
			RTS_SndData(10*ChangeMaterialbuf[0], FilementUnit1);	//It's ChangeMaterialbuf for show,instead of current_position[E_AXIS] in them.
			RTS_SndData(10*ChangeMaterialbuf[1], FilementUnit2);
			RTS_SndData(getActualTemp_celsius(H0),NozzleTemp);
			RTS_SndData(getTargetTemp_celsius(H0),NozzlePreheat);
			delay(2);
			RTS_SndData(ExchangePageBase + 65, ExchangepageAddr); 
			
		}
		else if(recdat.data[0] == 3)	//Move
		{
			//InforShowoStatus = false;
			AxisPagenum = 0;
			RTS_SndData(10*getAxisPosition_mm((axis_t)X), DisplayXaxis);
			RTS_SndData(10*getAxisPosition_mm((axis_t)Y), DisplayYaxis);
			RTS_SndData(10*getAxisPosition_mm((axis_t)Z), DisplayZaxis);
			delay(2);
			RTS_SndData(ExchangePageBase + 71, ExchangepageAddr); 
		}
		else if(recdat.data[0] == 4)	//Language
		{
			//Language change not supported
		}
		else if(recdat.data[0] == 5)	//Printer Information
		{
			RTS_SndData(WEBSITE_URL, CorpWebsite);
		}
		else if(recdat.data[0] == 6)// Diabalestepper
		{
			 enqueueCommands_P(PSTR("M84"));
			 RTS_SndData(11, FilenameIcon); 
		}
		break;

	case ReturnBack:
		 if(recdat.data[0] == 1)	 // return to the tool page
		 {
		 	InforShowStatus = false;
			RTS_SndData(ExchangePageBase + 63, ExchangepageAddr); 
		 }
		 if(recdat.data[0] == 2)	 // return to the Level mode page
		 {
			RTS_SndData(ExchangePageBase + 64, ExchangepageAddr); 
		 }
		break;
		
	case Bedlevel:
		#if (ENABLED(MachineCRX) && DISABLED(Force10SProDisplay)) || ENABLED(ForceCRXDisplay)
			if(recdat.data[0] == 1) // Top Left
			{
				waitway = 4;		//only for prohibiting to receive massage
				enqueueCommands_P((PSTR("G1 F100 Z3;"))); 
				enqueueCommands_P((PSTR("G1 X30 Y30 F5000")));
				waitway = 2;
				enqueueCommands_P((PSTR("G1 F100 Z0")));
				
			}
			else if(recdat.data[0] == 2) // Top Right
			{
				waitway = 4;		//only for prohibiting to receive massage
				enqueueCommands_P((PSTR("G1 F100 Z3"))); 
				enqueueCommands_P((PSTR("G1 X270 Y30 F5000")));
				waitway = 2;
				enqueueCommands_P((PSTR("G1 F100 Z0")));
			}
			else if(recdat.data[0] == 3) //  Centre
			{
				waitway = 4;		//only for prohibiting to receive massage
				enqueueCommands_P((PSTR("G1 F100 Z3"))); 
				enqueueCommands_P((PSTR("G1 X150 Y150 F5000")));
				waitway = 2;
				enqueueCommands_P((PSTR("G1 F100 Z0")));
			}
			else if(recdat.data[0] == 4) // Bottom Left
			{
				waitway = 4;		//only for prohibiting to receive massage
				enqueueCommands_P((PSTR("G1 F100 Z3")));
				enqueueCommands_P((PSTR("G1 X30 Y270 F5000")));
				waitway = 2;
				enqueueCommands_P((PSTR("G1 F100 Z0")));
			}
			else if(recdat.data[0] == 5) //  Bottom Right
			{
				waitway = 4;		//only for prohibiting to receive massage
				enqueueCommands_P((PSTR("G1 F100 Z3")));
				enqueueCommands_P((PSTR("G1 X270 Y270 F5000")));
				waitway = 2;
				enqueueCommands_P((PSTR("G1 F200 Z0")));
			}
			break;
		#else
			if(recdat.data[0] == 1)// Z-axis to home
			{
				// Disallow Z homing if X or Y are unknown
				if (!isAxisPositionKnown((axis_t)X) || !isAxisPositionKnown((axis_t)Y))
					enqueueCommands_P(PSTR("G28")); 
				else
					enqueueCommands_P(PSTR("G28 Z")); 
				enqueueCommands_P(PSTR("G1 F150 Z0.0")); 
				RTS_SndData(getZOffset_mm()*100, 0x1026); 
			}
			else if(recdat.data[0] == 2)// Z-axis to Up
			{
				//current_position[Z_AXIS] += 0.1; 
				//RTS_line_to_current(Z_AXIS);
				if (WITHIN((getZOffset_mm() +  0.1), Z_PROBE_OFFSET_RANGE_MIN, Z_PROBE_OFFSET_RANGE_MAX)) {
        	babystepAxis_steps(40, (axis_t)Z);
        	setZOffset_mm(getZOffset_mm() + 0.1);
					RTS_SndData(getZOffset_mm()*100, 0x1026);
					enqueueCommands_P(PSTR("M500")); 
				}
			}
			else if(recdat.data[0] == 3)// Z-axis to Down
			{
				if (WITHIN((getZOffset_mm() -  0.1), Z_PROBE_OFFSET_RANGE_MIN, Z_PROBE_OFFSET_RANGE_MAX)) {
        	babystepAxis_steps(-40, (axis_t)Z);
        	setZOffset_mm(getZOffset_mm() - 0.1);
					RTS_SndData(getZOffset_mm()*100, 0x1026);
					enqueueCommands_P(PSTR("M500")); 
				}
			}
			else if(recdat.data[0] == 4) 	// Assitant Level
			{
				//setLevelingActive(false); // FIX ME
				waitway = 4;		//only for prohibiting to receive massage
				if (!isPositionKnown())
					enqueueCommands_P((PSTR("G28")));
				waitway = 2;
				enqueueCommands_P((PSTR("G1 F100 Z0.0")));
				RTS_SndData(ExchangePageBase + 84, ExchangepageAddr); 
			}
			else if(recdat.data[0] == 5) 	// AutoLevel "Measuring" Button
			{
				waitway = 3;		//only for prohibiting to receive massage
				RTS_SndData(1, AutolevelIcon); 
				RTS_SndData(ExchangePageBase + 85, ExchangepageAddr); 
				enqueueCommands_P(PSTR(USER_GCODE_1)); 
			}
			else if(recdat.data[0] == 6) 	// Assitant Level ,  Centre 1
			{
				waitway = 4;		//only for prohibiting to receive massage
				enqueueCommands_P((PSTR("G1 F100 Z3;"))); 
				enqueueCommands_P((PSTR("G1 X150 Y150 F5000")));
				waitway = 2;
				enqueueCommands_P((PSTR("G1 F100 Z0")));
			}
			else if(recdat.data[0] == 7) 	// Assitant Level , Front Left 2
			{
				waitway = 4;		//only for prohibiting to receive massage
				enqueueCommands_P((PSTR("G1 F100 Z3;"))); 
				enqueueCommands_P((PSTR("G1 X30 Y30 F5000")));
				waitway = 2;
				enqueueCommands_P((PSTR("G1 F100 Z0")));
			}
			else if(recdat.data[0] == 8) 	// Assitant Level , Front Right 3
			{
				waitway = 4;		//only for prohibiting to receive massage
				enqueueCommands_P((PSTR("G1 F100 Z3;"))); 
				enqueueCommands_P((PSTR("G1 X270 Y30 F5000")));
				waitway = 2;
				enqueueCommands_P((PSTR("G1 F100 Z0")));
			}
			else if(recdat.data[0] == 9) 	// Assitant Level , Back Right 4
			{
				waitway = 4;		//only for prohibiting to receive massage
				enqueueCommands_P((PSTR("G1 F100 Z3;"))); 
				enqueueCommands_P((PSTR("G1 X270 Y270 F5000")));
				waitway = 2;
				enqueueCommands_P((PSTR("G1 F100 Z0")));
			}
			else if(recdat.data[0] == 10) 	// Assitant Level , Back Left 5
			{
				waitway = 4;		//only for prohibiting to receive massage
				enqueueCommands_P((PSTR("G1 F100 Z3;"))); 
				enqueueCommands_P((PSTR("G1 X30 Y270 F5000")));
				waitway = 2;
				enqueueCommands_P((PSTR("G1 F100 Z0")));
			}
			else if(recdat.data[0] == 11) 	// Autolevel switch
			{
				if(getLevelingActive())	//turn on the Autolevel
				{
					RTS_SndData(3, AutoLevelIcon);	
					enqueueCommands_P((PSTR("M420 S0")));
				}
				else//turn off the Autolevel
				{
					RTS_SndData(2, AutoLevelIcon);
					enqueueCommands_P((PSTR("M420 S1")));
				}
				RTS_SndData(getZOffset_mm()*100, 0x1026); 
			}
			
			RTS_SndData(10, FilenameIcon); 
			#endif
		break;

	case XYZEaxis:
		axis_t axis;
		float min,max;
		waitway = 4;
		if(recdat.addr == DisplayXaxis)
		{
			axis = X;
			min = X_MIN_POS;
			max = X_MAX_POS;
		}
		else if(recdat.addr == DisplayYaxis)
		{
			axis = Y;
			min = Y_MIN_POS;
			max = Y_MAX_POS;
		}
		else if(recdat.addr == DisplayZaxis)
		{
			axis = Z;
			min = Z_MIN_POS;
			max = Z_MAX_POS;
		}
		else if(recdat.addr == AutoZero)
		{
			if(recdat.data[0] == 3)	//autohome
			{
				waitway = 4;
				enqueueCommands_P((PSTR("G28")));
				enqueueCommands_P((PSTR("G1 F100 Z10")));
				InforShowStatus = AutohomeKey = true;
				AutoHomeIconNum = 0;
				RTS_SndData(ExchangePageBase + 74, ExchangepageAddr); 
				RTS_SndData(10,FilenameIcon);
			}
			else
			{
				AxisPagenum = recdat.data[0];
				waitway = 0;
			}
			break;
		}
		
		targetPos = ((float)recdat.data[0])/10;
		
		if (targetPos < min) targetPos = min;
		else if (targetPos > max) targetPos = max;
		setAxisPosition_mm(targetPos, axis);
		RTS_SndData(10*getAxisPosition_mm((axis_t)X), DisplayXaxis);
		RTS_SndData(10*getAxisPosition_mm((axis_t)Y), DisplayYaxis);
		RTS_SndData(10*getAxisPosition_mm((axis_t)Z), DisplayZaxis);
		
		delay(1);
		RTS_SndData(10, FilenameIcon); 
		waitway = 0;
		break;

	case Filement:
		
		#if ENABLED(MachineCR10SPro) || ENABLED(AddonFilSensor)
		/**************checking filement status during changing filement************/
		//if(RTS_CheckFilement(3)) break; //FIX ME
		#endif
		
		unsigned int IconTemp;
		if(recdat.addr == Exchfilement)
		{
			if(recdat.data[0] == 1)	// Unload filement1
			{
				original_extruder = getActiveTool();
				setActiveTool(E0, true);
				
				setAxisPosition_mm((getAxisPosition_mm(getActiveTool()) - ChangeMaterialbuf[0]), getActiveTool());

				if( NozzleTempStatus[1]== 0 && getActualTemp_celsius(H0) < (PLA_ABSModeTemp-5))
				{
					NozzleTempStatus[1] = 1; 
					RTS_SndData((int)PLA_ABSModeTemp, 0x1020);
					delay(5);
					RTS_SndData(ExchangePageBase + 66, ExchangepageAddr); 
					break;
				}
			}
			else if(recdat.data[0] == 2) // Load filement1
			{
				original_extruder = getActiveTool();
				setActiveTool(E0, true);
				setAxisPosition_mm((getAxisPosition_mm(getActiveTool()) + ChangeMaterialbuf[0]), getActiveTool());
				
				if( NozzleTempStatus[1]== 0 && getActualTemp_celsius(H0) < (PLA_ABSModeTemp-5))
				{
					NozzleTempStatus[1] = 1; 
					RTS_SndData((int)PLA_ABSModeTemp, 0x1020);
					delay(5);
					RTS_SndData(ExchangePageBase + 66, ExchangepageAddr); 
					break;
				}
			}
			else if(recdat.data[0] == 3) // Unload filement2
			{
				original_extruder = getActiveTool();
				setActiveTool(E1, true);
				
				setAxisPosition_mm((getAxisPosition_mm(getActiveTool()) - ChangeMaterialbuf[1]), getActiveTool());
				
				if( NozzleTempStatus[1]== 0 && getActualTemp_celsius(H0) < (PLA_ABSModeTemp-5))
				{
					NozzleTempStatus[1] = 1; 
					RTS_SndData((int)PLA_ABSModeTemp, 0x1020);
					delay(5);
					RTS_SndData(ExchangePageBase + 66, ExchangepageAddr); 
					break;
				}
			}
			else if(recdat.data[0] == 4) // Load filement2
			{
				original_extruder = getActiveTool();
				setActiveTool(E1, true);
				
				setAxisPosition_mm((getAxisPosition_mm(getActiveTool()) - ChangeMaterialbuf[1]), getActiveTool());
				
				if( NozzleTempStatus[1]== 0 && getActualTemp_celsius(H0) < (PLA_ABSModeTemp-5))
				{
					NozzleTempStatus[1] = 1; 
					RTS_SndData((int)PLA_ABSModeTemp, 0x1020);
					delay(5);
					RTS_SndData(ExchangePageBase + 66, ExchangepageAddr); 
					break;
				}
			}
			else if(recdat.data[0] == 5) // sure to heat
			{
				NozzleTempStatus[0] = 1;
				//InforShowoStatus = true;
				
				setTargetTemp_celsius((getTargetTemp_celsius(H0) >= PLA_ABSModeTemp? getTargetTemp_celsius(H0):  PLA_ABSModeTemp), H0) ;
				IconTemp = getActualTemp_celsius(H0) * 100/getTargetTemp_celsius(H0);
				if(IconTemp >= 100)
					IconTemp = 100;
				RTS_SndData(IconTemp, HeatPercentIcon);

				RTS_SndData(getActualTemp_celsius(H0), NozzleTemp);
				RTS_SndData(getTargetTemp_celsius(H0), NozzlePreheat); 
				delay(5);
				RTS_SndData(ExchangePageBase + 68, ExchangepageAddr); 
				break;
			}
			else if(recdat.data[0] == 6) //cancel to heat
			{
				NozzleTempStatus[1] = 0;
				RTS_SndData(ExchangePageBase + 65, ExchangepageAddr); 
				break;
			}
			else if(recdat.data[0] == 0xF1)	//Sure to cancel heating
			{
				//InforShowoStatus = true;
				NozzleTempStatus[0] = NozzleTempStatus[1] = 0;
				delay(1);
				RTS_SndData(ExchangePageBase + 65, ExchangepageAddr); 
				break;
			}
			else if(recdat.data[0] == 0xF0)	// not to cancel heating
				break;

			RTS_SndData(10*ChangeMaterialbuf[0], FilementUnit1);	//It's ChangeMaterialbuf for show,instead of current_position[E_AXIS] in them.
			RTS_SndData(10*ChangeMaterialbuf[1], FilementUnit2);
			setActiveTool(original_extruder, true);
		}
		else if(recdat.addr == FilementUnit1)
		{
			ChangeMaterialbuf[0] = ((float)recdat.data[0])/10;
		}
		else if(recdat.addr == FilementUnit2)
		{
			ChangeMaterialbuf[1] = ((float)recdat.data[0])/10;
		}
		break;

	case LanguageChoice:

			SERIAL_ECHOPAIR("\n ***recdat.data[0] =",recdat.data[0]);
			/*if(recdat.data[0]==1) {
				settings.save();
			}
			else {
				enqueueCommands_P(PSTR("M300"));
			}*/ // may at some point use language change screens to save eeprom explicitly
		break;
		
	case No_Filement:
		SERIAL_ECHO("\n No Filament");
		if(recdat.data[0] == 1)
		{
			/**************checking filement status during changing filement************/
			#if ENABLED(MachineCR10SPro) || ENABLED(AddonFilSensor)
				//if(RTS_CheckFilement(0)) break; //FIX ME
			#endif
			
			if(FilementStatus[0] == 1)	// check filement before starting to print
			{
				enqueueCommands_P(PSTR("M24"));
				for(int j = 0;j < 10;j++)	//clean screen.
					RTS_SndData(0,Printfilename+j);
				
				int filelen = strlen(CardRecbuf.Cardshowfilename[FilenamesCount]);
				filelen = (TEXTBYTELEN - filelen)/2;
				if(filelen > 0)
				{
					char buf[20];
					memset(buf,0,sizeof(buf));
					strncpy(buf,"         ",filelen);
					strcpy(&buf[filelen],CardRecbuf.Cardshowfilename[FilenamesCount]);
					RTS_SndData(buf, Printfilename);
				}
				else
					RTS_SndData(CardRecbuf.Cardshowfilename[FilenamesCount], Printfilename);
				delay(2);
				
				RTS_SndData(1+CEIconGrap,IconPrintstatus);	// 1 for Heating 
				delay(2);
				RTS_SndData(ExchangePageBase + 52, ExchangepageAddr); 
				TPShowStatus = InforShowStatus = true;
				PrinterStatusKey[0] = 1;
				PrinterStatusKey[1] = 3;
				CardCheckStatus[0] = 1;	// open the key of  checking card in  printing
				FilenamesCount = PrintStatue[1] = 0;

				FilementStatus[0]  =  0; // recover the status waiting to check filements
			}
			else if(FilementStatus[0] == 2)   // check filements status during printing
			{
				setHostResponse(1); //Send Resume host prompt command
				
				RTS_SndData(1+CEIconGrap,IconPrintstatus);
				PrintStatue[1] = 0;
				PrinterStatusKey[1] = 3;
				CardCheckStatus[0] = 1;	// open the key of  checking card in  printing
				RTS_SndData(ExchangePageBase + 52, ExchangepageAddr); 

				FilementStatus[0]  =  0; // recover the status waiting to check filements
			}
			else if(FilementStatus[0] == 3)
			{
				//RTS_SndData(current_position[E_AXIS], FilementUnit1);
				// RTS_SndData(current_position[E_AXIS], FilementUnit2);
				RTS_SndData(ExchangePageBase + 65, ExchangepageAddr); 
			}
		}
		else if(recdat.data[0] == 0)
		{
			if(FilementStatus[0] == 1)
			{
				RTS_SndData(ExchangePageBase + 46, ExchangepageAddr); 
				PrinterStatusKey[0] = 0;
			}
			else if(FilementStatus[0] == 2) // like the pause
			{
				RTS_SndData(ExchangePageBase + 54, ExchangepageAddr); 
			}
			else if(FilementStatus[0] == 3)
			{
				RTS_SndData(ExchangePageBase + 65, ExchangepageAddr); 
			}
			FilementStatus[0]  =  0; // recover the status waiting to check filements
		}
		break;
	#if ENABLED(POWER_LOSS_RECOVERY)
	case PwrOffNoF:
		//SERIAL_ECHO("\n   recdat.data[0] ==");
		//SERIAL_ECHO(recdat.data[0]);
		//SERIAL_ECHO("\n   recdat.addr ==");
		//SERIAL_ECHO(recdat.addr);
		char cmd1[30];
		if(recdat.data[0] == 1)// Yes:continue to print the 3Dmode during power-off.
		{
			if (power_off_commands_count > 0) {
				sprintf_P(cmd1, PSTR("M190 S%i"), power_off_info.target_temperature_bed);
				enqueue_and_echo_command(cmd1);
				sprintf_P(cmd1, PSTR("M109 S%i"), power_off_info.target_temperature[0]);
				enqueue_and_echo_command(cmd1);
				enqueueCommands_P(PSTR("M106 S255"));
				sprintf_P(cmd1, PSTR("T%i"), power_off_info.saved_extruder);
				enqueue_and_echo_command(cmd1);
				power_off_type_yes = 1;

				#if FAN_COUNT > 0
				for (uint8_t i = 0; i < FAN_COUNT; i++) fanSpeeds[i] = FanOn;
				#endif
				FanStatus = false;
			
				PrintStatue[1] = 0;
				PrinterStatusKey[0] = 1; 
				PrinterStatusKey[1] = 3;
				PoweroffContinue = true;
				TPShowStatus = InforShowStatus = true;
				CardCheckStatus[0] = 1;	// open the key of  checking card in  printing
				
					RTS_SndData(1+CEIconGrap,IconPrintstatus);
					RTS_SndData(ExchangePageBase + 52, ExchangepageAddr); 
				
				//card.startFileprint();
				//print_job_timer.power_off_start();
			}
		}
		else if(recdat.data[0] == 2)// No
		{
			InforShowStatus = true;
			TPShowStatus = false;
			RTS_SndData(ExchangePageBase + 45, ExchangepageAddr); //exchange to 45 page

			card.stopSDPrint();
			clear_command_queue();
			quickstop_stepper();
			thermalManager.disable_all_heaters();
			
			#if ENABLED(SDSUPPORT) && ENABLED(POWEROFF_SAVE_SD_FILE)
				card.openPowerOffFile(power_off_info.power_off_filename, O_CREAT | O_WRITE | O_TRUNC | O_SYNC);
				power_off_info.valid_head = 0;
				power_off_info.valid_foot = 0;
				if (card.savePowerOffInfo(&power_off_info, sizeof(power_off_info)) == -1)
				{
					SERIAL_ECHOLN("Stop to Write power off file failed.");
				}
				card.closePowerOffFile();
				power_off_commands_count = 0;
			#endif

			wait_for_heatup = false;
			PrinterStatusKey[0] = 0;
			delay(500);	//for system

		}
		break;
	#endif
	case Volume:
		if(recdat.data[0] < 0) VolumeSet = 0;
		else if(recdat.data[0] > 255 ) VolumeSet = 0xFF;
		else VolumeSet = recdat.data[0];

		if(VolumeSet == 0)
		{
			RTS_SndData(0, VolumeIcon);
			RTS_SndData(9, SoundIcon);
		}
		else
		{
			RTS_SndData((VolumeSet+1)/32 - 1, VolumeIcon);
			RTS_SndData(8, SoundIcon);
		}
		//eeprom_write_byte((unsigned char*)FONT_EEPROM+4, VolumeSet);
		RTS_SndData(VolumeSet<<8, SoundAddr+1);
		break;
		
	case Filename :
	       //if(card.cardOK && recdat.data[0] > 0 && recdat.data[0] <= CardRecbuf.Filesum && recdat.addr != 0x20D2)
		/*SERIAL_ECHO("\n   recdat.data[0] ==");
		SERIAL_ECHO(recdat.data[0]);
		SERIAL_ECHO("\n   recdat.addr ==");
		SERIAL_ECHO(recdat.addr); */
		
		//if(card.cardOK && recdat.addr == FilenameChs)  //FIX ME
				if (false)
	       {
	       	if(recdat.data[0] > CardRecbuf.Filesum) break;
			
			CardRecbuf.recordcount = recdat.data[0] - 1;
			for(int j = 0;j < 10;j++)
				RTS_SndData(0,Choosefilename+j);
			int filelen = strlen(CardRecbuf.Cardshowfilename[CardRecbuf.recordcount]);
			filelen = (TEXTBYTELEN - filelen)/2;
			if(filelen > 0)
			{
				char buf[20];
				memset(buf,0,sizeof(buf));
				strncpy(buf,"         ",filelen);
				strcpy(&buf[filelen],CardRecbuf.Cardshowfilename[CardRecbuf.recordcount]);
				RTS_SndData(buf, Choosefilename);
			}
			else
				RTS_SndData(CardRecbuf.Cardshowfilename[CardRecbuf.recordcount], Choosefilename);
			
			for(int j = 0;j < 8;j++)
				RTS_SndData(0,FilenameCount+j);
			char buf[20];
			memset(buf,0,sizeof(buf));
			sprintf(buf,"%d/%d",(int)recdat.data[0], CardRecbuf.Filesum);
			RTS_SndData(buf, FilenameCount);
			delay(2);
			for(int j = 1;j <= CardRecbuf.Filesum;j++)
			{
				RTS_SndData((unsigned long)0xFFFF,FilenameNature + j*16);		// white
				RTS_SndData(10,FilenameIcon1+j);	//clean
			}
				
			RTS_SndData((unsigned long)0x87F0,FilenameNature + recdat.data[0]*16);	// Light green
			RTS_SndData(6,FilenameIcon1 + recdat.data[0]);	// show frame
			
	       } 
		else if(recdat.addr == FilenamePlay)
		{
			//if(recdat.data[0] == 1 && card.cardOK)	//for sure //FIX ME
			if(false)
			{
				if(CardRecbuf.recordcount < 0)
					break;

				//SERIAL_ECHO("*************suceed1**********");
				char cmd[30];
				char* c;
				sprintf_P(cmd, PSTR("M23 %s"), CardRecbuf.Cardfilename[CardRecbuf.recordcount]);
				for (c = &cmd[4]; *c; c++) *c = tolower(*c);

				FilenamesCount = CardRecbuf.recordcount;
				memset(cmdbuf,0,sizeof(cmdbuf));
				strcpy(cmdbuf,cmd);
 
				#if ENABLED(MachineCR10SPro) || ENABLED(AddonFilSensor)
				/**************checking filement status during printing beginning ************/
					//if(RTS_CheckFilement(1)) break; //FIX ME
				#endif
					
				//InforShowoStatus = true;
				enqueueCommands_P(PSTR("M24"));
				for(int j = 0;j < 10;j++)	//clean screen.
					RTS_SndData(0,Printfilename+j);
				
				int filelen = strlen(CardRecbuf.Cardshowfilename[CardRecbuf.recordcount]);
				filelen = (TEXTBYTELEN - filelen)/2;
				if(filelen > 0)
				{
					char buf[20];
					memset(buf,0,sizeof(buf));
					strncpy(buf,"         ",filelen);
					strcpy(&buf[filelen],CardRecbuf.Cardshowfilename[CardRecbuf.recordcount]);
					RTS_SndData(buf, Printfilename);
				}
				else
					RTS_SndData(CardRecbuf.Cardshowfilename[CardRecbuf.recordcount], Printfilename);
				delay(2);
				
				#if FAN_COUNT > 0
				for (uint8_t i = 0; i < FAN_COUNT; i++) setTargetFan_percent(100, (fan_t)i);
				#endif
				FanStatus = false;
			
				
					RTS_SndData(1+CEIconGrap,IconPrintstatus);	// 1 for Heating 
					delay(2);
					RTS_SndData(ExchangePageBase + 52, ExchangepageAddr); 
				
				TPShowStatus = InforShowStatus = true;
				PrintStatue[1] = 0;
				PrinterStatusKey[0] = 1;
				PrinterStatusKey[1] = 3;
				CardCheckStatus[0] = 1;	// open the key of  checking card in  printing
			}
			else if(recdat.data[0] == 0) //	return to main page
			{
				InforShowStatus = true;
				TPShowStatus = false;
			}
		}
		break;
		
	default:
	       break;
    	}
	
	memset(&recdat,0 , sizeof(recdat));
	recdat.head[0] = FHONE;
	recdat.head[1] = FHTWO;
}

void onPrinterKilled(PGM_P const msg) {}
  void onMediaInserted() {};
  void onMediaError() {};
  void onMediaRemoved() {};
  void onPlayTone(const uint16_t frequency, const uint16_t duration) {}
  void onPrintTimerStarted() {
    #if ENABLED(POWER_LOSS_RECOVERY)
			if(PoweroffContinue)
			{
					enqueue_and_echo_command(power_off_commands[0]);
					enqueue_and_echo_command(power_off_commands[1]);
					enqueue_and_echo_commands_P((PSTR("G28 X0 Y0")));
			}
		#endif
		if(PrinterStatusKey[1] == 3)
		{
			PrinterStatusKey[1] = 0;
			InforShowStatus = true;	
			rtscheck.RTS_SndData(2+CEIconGrap,IconPrintstatus);	
			delay(1);
			rtscheck.RTS_SndData(ExchangePageBase + 53, ExchangepageAddr);
			CardCheckStatus[0] = 1;	// open the key of  checking card in  printing
			FilementStatus[1] = 1; 	//begin to check filement status.
			//SERIAL_ECHOPAIR("\n ***M109 Status[1] =",FilementStatus[1]);
		}
		//SERIAL_ECHOPAIR("\n ***PrinterStatusKey[1] =",PrinterStatusKey[1]);
	}
	
  void onPrintTimerPaused() {
		rtscheck.RTS_SndData(ExchangePageBase + 87, ExchangepageAddr); //Display Pause Screen
	}
  void onPrintTimerStopped() {}
  void onFilamentRunout() {
		waitway = 5;		//reject to receive cmd and jump to the corresponding page
		PrintStatue[1] = 1;	// for returning the corresponding page
		Checkfilenum=0;
		FilementStatus[1] = 0;
		PrinterStatusKey[1] = 4;		
		TPShowStatus = false;
	}
	void onFilamentRunout(extruder_t extruder) {}
  void onUserConfirmRequired(const char * const msg) {}
  void onStatusChanged(const char * const msg) {}
  void onFactoryReset() {}
  void onLoadSettings() {}
  void onStoreSettings() {}
	void onMeshUpdate(const uint8_t xpos, const uint8_t ypos, const float zval) {
		rtscheck.RTS_SndData(zval *10000, AutolevelVal + (15-(xpos*ypos)-1)*2);
		rtscheck.RTS_SndData(15-(xpos*ypos),AutolevelIcon);
	};

} // NAMESPACE EXT_UI