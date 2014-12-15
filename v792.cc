#include <stdio.h>
#include <signal.h>

#include <CAENVMElib.h>
#include <TSystem.h>
#include <TFile.h>
#include <TH1.h>
#include <TTree.h>
#include <TCanvas.h>
#include <TApplication.h>

#define QDCBASE 0xAD020000

volatile int stop_flag=0; 
void sigint_handler(int sig)
{
 signal( SIGINT, SIG_IGN );
 printf("Received SIGINT\n");
 stop_flag = -1;
 return;
}

int main(int argc, char **argv)
{
  int EventNumber = 0;
  int MaxEventNumber = -1;
  
  TApplication* theApp = new TApplication("App", 0, 0);
  TH1F *h1 = new TH1F("h1","QDC ch0",3000,0,3000);
  
  //TCanvas *c1 = new TCanvas("c1","Event Display",600,800);
  TCanvas *c1 = new TCanvas("c1","Event Display");
  h1->Draw();
  c1->SetLogy();
  c1->Update();

  std::string outfile("test.root");
  if(argc>=2){
    outfile = argv[1];
  }
  if(argc==3){
    MaxEventNumber = atoi(argv[2]);
  }

  //ROOT output file
  TFile *file = new TFile(outfile.c_str(),"RECREATE");
  TTree *tree = new TTree("tree","tree");
  int QDC[32];
  tree->Branch("QDC",QDC,"QDC[32]/I");
  
  printf("Output File: %s\n",outfile.c_str());
  printf("Max Event Number: %d\n",MaxEventNumber);

  //VME open--------------------------------------------------------------
  CVBoardTypes VMEBoard = cvV1718;
  short int Link = 0;
  short int Device = 0;
  int BHandle;
  uint32_t data;
  uint32_t buf[34];

  CAENVME_Init(VMEBoard, Device, Link, &BHandle) ;

  CAENVME_SetOutputConf(BHandle, cvOutput0, cvDirect, cvActiveHigh, cvManualSW);
  CAENVME_SetOutputConf(BHandle, cvOutput1, cvDirect, cvActiveHigh, cvMiscSignals);
  CAENVME_SetOutputConf(BHandle, cvOutput2, cvDirect, cvActiveHigh, cvManualSW);
  CAENVME_SetOutputConf(BHandle, cvOutput3, cvDirect, cvActiveHigh, cvManualSW);
  CAENVME_SetOutputConf(BHandle, cvOutput4, cvDirect, cvActiveHigh, cvManualSW);

  uint32_t period = 100;
  uint32_t width = 1;
  CAENVME_SetPulserConf(BHandle, cvPulserA, period, width, cvUnit410us, 0, cvManualSW, cvManualSW);
  CAENVME_StartPulser(BHandle, cvPulserA);

  //V792 setting-----------------------------------------------------------
  data=0x80;
  CAENVME_WriteCycle(BHandle, QDCBASE+0x1006, &data, cvA32_U_DATA, cvD16);
  CAENVME_WriteCycle(BHandle, QDCBASE+0x1008, &data, cvA32_U_DATA, cvD16);
  data=180;    //60 < Iped < 255 (default: 180)
  CAENVME_WriteCycle(BHandle, QDCBASE+0x1060, &data, cvA32_U_DATA, cvD16);
  data=0x18;   //OVER RANGE, LOW THR. disabled
  CAENVME_WriteCycle(BHandle, QDCBASE+0x1034, &data, cvA32_U_DATA, cvD16);
  
  signal(SIGINT, sigint_handler);
    
  volatile int timeout_flag=0;
  time_t last_update_time=0;
  time_t update_interval=1;  //second

  while(stop_flag==0){

    //Stop veto
    if(timeout_flag==0) CAENVME_PulseOutputRegister(BHandle, cvOut0Bit);
    
    //Wait trigger
    timeout_flag=1;
    for(int i=0;i<=100000;i++){
      //Status check
      CAENVME_ReadCycle(BHandle, QDCBASE+0x100E, &data, cvA32_U_DATA, cvD16);
      if( (data&0x1) == 1) {
	timeout_flag=0;
	break;
      }
    }
    
    //Timout
    if( timeout_flag==1 ){
      printf("TimeOut!\n");
      continue;
    }

    EventNumber++; 
    
    for(int i=0;i<32;i++){
      QDC[i]=-1;
    }
   
    int nread=0;
    //CAENVME_BLTReadCycle(BHandle, QDCBASE, buf, 34*4, cvA32_S_BLT, cvD32, &nread);
    CAENVME_MBLTReadCycle(BHandle, QDCBASE, buf, 34*4, cvA32_S_MBLT, &nread);
    
    int ch=0;
    for(int i=0;i<34;i++){
      int type = (buf[i]>>24)&0x7;
      switch (type){
      case 0: //valid datum
	//printf("%08x\n",buf[i]);
	ch = (buf[i]>>16)&0x1f;
	if(0<= ch && ch<=31) QDC[ch]=buf[i]&0xfff;
	if(ch==0) h1->Fill(QDC[ch]);
	break;
      case 2: //header
	//printf("CNT[5:0]=%d\n",(buf[i]>>8)&0x3f);
	break;
      case 4: //end of block
	break;
      case 6: //not valid datum
	break;
      default: //reserved
	printf("unknown data type!! %08x\n",buf[i]);
	break;
      }
    }
    
    //clear data
    data=0x4;
    CAENVME_WriteCycle(BHandle, QDCBASE+0x1032, &data, cvA32_U_DATA, cvD16);
    CAENVME_WriteCycle(BHandle, QDCBASE+0x1034, &data, cvA32_U_DATA, cvD16);
    
    tree->Fill();
    
    time_t now = time(NULL);
    
    gSystem->ProcessEvents();
    if(now-last_update_time >= update_interval){
      last_update_time = now;
      //h1->Draw();
      c1->Modified();
      c1->Update();
    }
    
    if(MaxEventNumber==EventNumber) stop_flag = 1;
  }
  
  file->Write();
  file->Close();

  CAENVME_End (BHandle);
  theApp->Run(kTRUE);
  return 0 ;
}

