#Create a simulator object
set ns [new Simulator]

#####################################################
#Define different colors for data flows
$ns color 1 Blue
$ns color 2 Red
$ns color 3 Green

#Open the NAM trace file
set nf [open out.nam w]
$ns namtrace-all $nf
#######################################################

#Open a trace file

$ns trace-all [open all.out w]


#Define a 'finish' procedure
proc finish {} {
        global ns 
        $ns flush-trace
#		exec nam out.nam &
        exit 0
}

#Create three nodes
set n0 [$ns node]
set n1 [$ns node]
set n2 [$ns node]
set n3 [$ns node]
set n4 [$ns node]
set n5 [$ns node]


#
#n0-------n1-----n2-----n3
#         |      |
#         |      |
#         n4    n5
#




#Connect the nodes with two links

$ns duplex-link $n0 $n1 1000Mb 10ms DRR
$ns duplex-link $n1 $n2 100Mb 10ms DRR
$ns duplex-link $n2 $n3 1000Mb 10ms  DRR
$ns duplex-link $n4 $n1 1000Mb 10ms  DRR
$ns duplex-link $n5 $n2 1000Mb 10ms  DRR

$ns duplex-link-op $n0 $n1 orient right
$ns duplex-link-op $n1 $n2 orient right
$ns duplex-link-op $n2 $n3 orient right
$ns duplex-link-op $n4 $n1 orient up
$ns duplex-link-op $n5 $n2 orient up

#########################################################
#Create two ping agents and attach them to the nodes n0 and n3
set p0 [new Agent/hybChirpSnd]

$p0 set lowrate_ 1000000.0   # lowest rate in chirp train
$p0 set highrate_ 12839184.645 # highest rate in chirp train 1M*1.2^14
$p0 set avgrate_ 4000000.0    # average data probing rate
$p0 set packetSize_ 1000  	 # probe packet size
$p0 set spreadfactor_ 1.2    # spread factor within chirp
$p0 set a_ 1.33
$p0 set m_ 4.0
$p0 set n_ 3

$p0 set fid_ 1
$ns attach-agent $n0 $p0


#########################################################

set p1 [new Agent/hybChirpRcv]

$p1 set packetSize_ 64  	 # feedback packet size
$p1 set num_inst_bw 7   	 # number of chirp estimates to smooth over
$p1 set decrease_factor 3 	 # excursion detection parameter
$p1 set busy_period_thresh 5 # excursion detection parameter 
$p1 set fid_ 2

$ns attach-agent $n3 $p1


###########################################################


#Connect the two agents
$ns connect $p0 $p1


###########################################################
# CBR CROSS TRAFFIC

set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 1000
$cbr set rate_ 90Mb #to get 1 percent of bandwidth

set udp [new Agent/UDP]
$udp set fid_ 3
$ns attach-agent $n4 $udp
$cbr attach-agent $udp
set null0 [new Agent/Null] 
$ns attach-agent $n5 $null0
 
$ns connect $udp $null0


###############################################
#Schedule events
#Note: must start both the hybChirp sender and receiver
$ns at 0.1 "$p0 start"
$ns at 0.1 "$p1 start"
$ns at 20.0 "$p0 stop"

$ns at 0.0 "$cbr start"
$ns at 20.0 "$cbr stop"
$ns at 22.0 "finish"

#Run the simulation
$ns run



