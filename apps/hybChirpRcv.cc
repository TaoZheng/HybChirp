#include "pathChirpRcv.h"

int hdr_pathChirpRcv::offset_;

static class pathChirpHeaderClass : public PacketHeaderClass {
public:
	pathChirpHeaderClass() : PacketHeaderClass("PacketHeader/pathChirpRcv", 
					      sizeof(hdr_pathChirpRcv)) {
		bind_offset(&hdr_pathChirpRcv::offset_);
	}
} class_pathChirphdrRcv;


static class pathChirpRcvClass : public TclClass {
public:
	pathChirpRcvClass() : TclClass("Agent/pathChirpRcv") {}
	TclObject* create(int, const char*const*) {
		return (new pathChirpAgentRcv());
	}
} class_pathChirpRcv;

/* constructor*/
pathChirpAgentRcv::pathChirpAgentRcv() : Agent(PT_PATHCHIRPRCV) 
{ 
      bind("packetSize_", &size_);
      bind("num_inst_bw", &num_inst_bw);
      bind("decrease_factor", &decrease_factor);
      bind("busy_period_thresh", &busy_period_thresh);
  
}


/* initialization */
void pathChirpAgentRcv::start()
{
  running_=0;//initializing to zero
  inst_bw_estimates=(double *)calloc((int)(num_inst_bw),sizeof(double));
  inst_bw_count=0;

  total_inst_bw=0;
  inst_head=0;
  congestionflag_=0;
  chirpType=EXP;
  increment=0.0;
  pkt_sum = 0; //记录接收到的包总数
}

/* resets connection, will recalculate parameters etc with next received packet */
void pathChirpAgentRcv::resetting()
{
  running_=0;//initializing to zero

  congestionflag_=0;


  free(qing_delay);
  free(rates);
  free(iat);
  free(av_bw_per_pkt);
 return;
}

/* writes out smooted available bandwidth estimate */
void pathChirpAgentRcv::write_instant_bw()
{
  double inst_av_bw;
  double inst_mean=0.0;
num_inst_bw = 7;
  /*taking latest value and removing old one*/
  inst_av_bw=computeavbw();

  total_inst_bw+=inst_av_bw;
  total_inst_bw-=inst_bw_estimates[inst_head];
  
  inst_bw_estimates[inst_head]=inst_av_bw;
  
  inst_head++;
  /*circular buffer*/
  if (inst_head>=num_inst_bw)
    inst_head=0;
  
  if (inst_bw_count>num_inst_bw)
    {
      inst_mean=total_inst_bw/((double) num_inst_bw);
      /* write to file */
      fprintf(stdout,"%f %fM %fM (type=%d)\n",Scheduler::instance().clock(),inst_mean/1000000.0, pkt_sum*packetSize_/1000000.0, chirpType);
//	  fprintf(stdout,"%f %fM\n",Scheduler::instance().clock(),inst_mean/1000000.0);
      sendpkt(inst_mean,congestionflag_);
    }
  else  
    inst_bw_count++;

  //reset congestion flag
  congestionflag_=0;

}


/* the pathChirp algorithm*/
double pathChirpAgentRcv::computeavbw()
{

/*
int i=0;
for (i=0; i<num_interarrival; i++)
 printf("%f ", qing_delay[i]);
 printf("\n");
*/

  /* compute the instantaneous bandwidth during this chirp, new algorithm */ 
  int cur_loc=0;/* current location in chirp */
  int cur_inc=0;/* current location where queuing delay increases */
  int count;
  double inst_bw=0.0,sum_iat=0.0,max_q=0.0;
  printf("包个数：%d\n", num_interarrival); 
  
  if (congestionflag_==1)
    {
      inst_bw=0.0;
      return(inst_bw);//returning zero available bandwidth
    }
  
  /* set all av_bw_per_pkt to zero */
  memset(av_bw_per_pkt, 0, (int)(num_interarrival) * sizeof(double));
  
  /*find first place with an increase in queuing delay*/
  while((qing_delay[cur_inc]>=qing_delay[cur_inc+1]) && (cur_inc<num_interarrival))
    cur_inc++;
  
  cur_loc=cur_inc+1; 
  
  /* go through all delays */
  
  while(cur_loc<=num_interarrival)
    {
      /* find maximum queuing delay between cur_inc and cur_loc*/
      if (max_q<(qing_delay[cur_loc]-qing_delay[cur_inc]))
	max_q=qing_delay[cur_loc]-qing_delay[cur_inc];
      
      /* check if queuing delay has decreased to "almost" what it was at cur_inc*/
      if (qing_delay[cur_loc]-qing_delay[cur_inc]<(max_q/decrease_factor))
	{
	  if (cur_loc-cur_inc>=busy_period_thresh)
	    {
	      for (count=cur_inc;count<=cur_loc-1;count++)
		{
		  /*mark all increase regions between cur_inc and cur_loc as having
		    available bandwidth equal to their rates*/
		  if (qing_delay[count]<qing_delay[count+1])
		    av_bw_per_pkt[count]=rates[count];
		}
	    }
	  /* find next increase point */
	  cur_inc=cur_loc;
	  while(qing_delay[cur_inc]>=qing_delay[cur_inc+1] && cur_inc<num_interarrival)
	    cur_inc++;
	  cur_loc=cur_inc;
	  /* reset max_q*/
	  max_q=0.0;
	}

      cur_loc++;

    }
  /*mark the available bandwidth during the last excursion as the rate at its beginning*/
  if(cur_inc==num_interarrival)
    cur_inc--;
  
  for(count=cur_inc;count<num_interarrival;count++)
    {
      av_bw_per_pkt[count]=rates[cur_inc];
    }
  
  
  /* all unmarked locations are assumed to have available 
     bandwidth described by the last excursion */
  
  for(count=0;count<num_interarrival;count++)
    {

      if (av_bw_per_pkt[count]<1.0)
	av_bw_per_pkt[count]=rates[cur_inc];
      sum_iat+=iat[count];
      inst_bw+=av_bw_per_pkt[count]*iat[count];
    }

  inst_bw=inst_bw/sum_iat;
 
  if (cur_inc > 0) {
    	inst_bw = rates[cur_inc-1]*0.5 + rates[cur_inc]*0.5;
  }else {
    	inst_bw = rates[cur_inc]; //当初忘了加这句使得结果异常奇怪！！！
  }
  
  inst_bw=rates[cur_inc];
  
  return(inst_bw);
  
}

/* on receiving packet run the pathchirp algorithm */
void pathChirpAgentRcv::recv(Packet* pkt, Handler*)
{
 pkt_sum++; //收到一个包，增加计数
  // Access the IP header for the received packet:
  hdr_ip* hdrip = hdr_ip::access(pkt);
  
  // Access the pathChirp header for the received packet:
  hdr_pathChirpSnd* hdr = hdr_pathChirpSnd::access(pkt);
  
  // check if in brdcast mode
  if ((u_int32_t)hdrip->daddr() == IP_BROADCAST) {
      Packet::free(pkt);
      return;
  }

  //on receipt of first packet allocate memory for variables
  if (running_==0)
    {
      num_interarrival=hdr->num_pkts_per_chirp-1;
      lowrate_=hdr->lowrate_;
      spreadfactor_=hdr->spreadfactor_;
      packetSize_ = hdr->packetSize_;
	  chirpType = hdr->chirpType;
	  increment=hdr->increment;
      allocatemem();
      running_=1;
    }
  else
    {
      //if there is a change in parameters then reset
      if ((lowrate_!=hdr->lowrate_) || (num_interarrival!=(hdr->num_pkts_per_chirp-1)) || (chirpType!=hdr->chirpType) || (increment!=hdr->increment))
	{
	  resetting();
      num_interarrival=hdr->num_pkts_per_chirp-1;
      lowrate_=hdr->lowrate_;
      spreadfactor_=hdr->spreadfactor_;
      packetSize_ = hdr->packetSize_;
	  chirpType = hdr->chirpType;
	  increment=hdr->increment;
      allocatemem();
      running_=1;
	}
    }


   // If packet is missing then don't run the pathChirp 
   // algo, mark congested flag
  // NOTE: We are ignoring the case when more than num_pkts_per_chirp consecutive packets are lost (will rarely occur)
 	  if ((hdr->pktnum_ - prevpktnum_ >1) || ((prevpktnum_<hdr->num_pkts_per_chirp-1) && (hdr->pktnum_ <= prevpktnum_)))
	  {
	    congestionflag_=1;
	  }
	
	  // if we are already in the next chirp because the last few packets of the previous one was dropped then write the bandwidth estimate
	  if ((prevpktnum_<hdr->num_pkts_per_chirp-1) && (hdr->pktnum_ < prevpktnum_))
	    {
	       write_instant_bw();
	    }

   //record queuing delay
   qing_delay[hdr->pktnum_]=Scheduler::instance().clock() - hdr->send_time;
   

   if (hdr->pktnum_ == (hdr->num_pkts_per_chirp-1))
     write_instant_bw();
     
   prevpktnum_=hdr->pktnum_;

    // free memory
    Packet::free(pkt);

}


/* send out a packet with timestamp and appropriate  packet number */
void pathChirpAgentRcv::sendpkt(double inst_bw,int loss)
{

      // Create a new packet
      Packet* pkt = allocpkt();

      // Access the pathChirp header for the new packet:
      hdr_pathChirpRcv* hdr = hdr_pathChirpRcv::access(pkt);

      // Store the current time in the 'send_time' field
      hdr->inst_bw=inst_bw;

      //hdr->size=packetSize_;
      hdr->loss = loss;

      // Send the packet
      send(pkt, 0);

      return;
}
   

/* used by tcl script */
int pathChirpAgentRcv::command(int argc, const char*const* argv)
{
	if (argc == 2) {
	  if (strcmp(argv[1], "start") == 0) {
	    start();
	    return (TCL_OK);
	  }
	}
	return (Agent::command(argc, argv));
}

/* allocating memory */
void pathChirpAgentRcv::allocatemem()
{
	if (chirpType == EXP) allocatemem1();
	else if (chirpType == LIN) allocatemem2();
}


void pathChirpAgentRcv::allocatemem1()
{

  int count;
  

  qing_delay=(double *)calloc((int)(num_interarrival+1),sizeof(double));

  rates=(double *)calloc((int)(num_interarrival),sizeof(double));
  iat=(double *)calloc((int)(num_interarrival),sizeof(double));

  av_bw_per_pkt=(double *)calloc((int)(num_interarrival),sizeof(double));

  /* initialize variables */
  
  rates[0]=lowrate_;


  iat[0]=8*((double) packetSize_)/rates[0];

  /* compute instantaneous rates within chirp */
  for (count=1;count<=num_interarrival-1;count++)
    {

      rates[count]=rates[count-1]*spreadfactor_;
      iat[count]=iat[count-1]/spreadfactor_;

    }

  return;

}
//////////////////////////////////////////////////////////////////////////////
void pathChirpAgentRcv::allocatemem2()
{

  int count;
  

  qing_delay=(double *)calloc((int)(num_interarrival+1),sizeof(double));

  rates=(double *)calloc((int)(num_interarrival),sizeof(double));
  iat=(double *)calloc((int)(num_interarrival),sizeof(double));

  av_bw_per_pkt=(double *)calloc((int)(num_interarrival),sizeof(double));

  /* initialize variables */
  
  rates[0]=lowrate_;


  iat[0]=8*((double) packetSize_)/rates[0];

  /* compute instantaneous rates within chirp */
  for (count=1;count<=num_interarrival-1;count++)
    {
      rates[count]=rates[count-1]+increment;
      iat[count]=8*((double) packetSize_)/rates[count];
    }
  return;

}


