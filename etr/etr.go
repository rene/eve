// Copyright (c) 2017 Zededa, Inc.
// All rights reserved.

// This implements the ETR functionality. Listens on UDP destination port 4341 for
// receiving packets behind the same NAT. Cross NAT packets are captured using pfring
// listening for packets with source port 4341 and destination ephemeral port received
// from lispers.net.

package etr

import (
	"bytes"
	//"crypto/aes"
	"crypto/cipher"
	"fmt"
	"github.com/google/gopacket"
	"github.com/google/gopacket/afpacket"
	"github.com/google/gopacket/layers"
	"github.com/google/gopacket/pcap"
	"golang.org/x/net/bpf"
	//"github.com/google/gopacket/pfring"
	"github.com/zededa/go-provision/types"
	"github.com/zededa/lisp/dataplane/dptypes"
	"github.com/zededa/lisp/dataplane/fib"
	"log"
	"net"
	"syscall"
	"time"
	"unsafe"
)

// Status and metadata of different ETR threads currently running
var EtrTable dptypes.EtrTable
var deviceNetworkStatus types.DeviceNetworkStatus
var debug bool = false

const (
	uplinkFileName  = "/var/run/zedrouter/DeviceNetworkStatus/global.json"
	etrNatPortMatch = "udp dst port %d and udp src port 4341"
)

func InitETRStatus(debugFlag bool) {
	debug = debugFlag
	EtrTable.EphPort = -1
	EtrTable.EtrTable = make(map[string]*dptypes.EtrRunStatus)
}

func StartEtrNonNat() {
	log.Println("StartEtrNonNat: Starting ETR thread on port 4341")
	// create a udp server socket and start listening on port 4341
	// XXX Using ipv4 underlay for now. Will have to figure out v6 underlay case.
	etrServer, err := net.ResolveUDPAddr("udp4", ":4341")
	if err != nil {
		log.Fatal("StartEtrNonNat: Error resolving ETR socket address: %s\n", err)
	}
	serverConn, err := net.ListenUDP("udp4", etrServer)
	if err != nil {
		log.Printf("StartEtrNonNat: Unable to start ETR server on :4341: %s\n", err)

		// try after 2 seconds
		go func() {
			time.Sleep(2 * time.Second)
			StartEtrNonNat()
		}()
		return
	}

	// Create a raw socket for injecting decapsulated packets
	fd, err := syscall.Socket(syscall.AF_INET6, syscall.SOCK_RAW, syscall.IPPROTO_RAW)
	if err != nil {
		serverConn.Close()
		log.Fatal(
			"StartEtrNonNat: Creating ETR raw socket for packet injection failed: %s\n",
			err)
		return
	}

	// start processing packets. This loop should never end.
	go ProcessETRPkts(fd, serverConn)
}

func HandleDeviceNetworkChange(deviceNetworkStatus types.DeviceNetworkStatus) {
	ipv4Found, ipv6Found := false, false
	ipv4Addr, ipv6Addr := net.IP{}, net.IP{}

	if debug {
		log.Printf("HandleDeviceNetworkChange: Free uplinks have changed" +
			" new ipv4 & ipv6 source addresses will be picked\n")
	}
	links := types.GetUplinkFreeNoLocal(deviceNetworkStatus)

	// Collect the interfaces that are still valid
	// Create newly required ETR instances
	validList := make(map[string]bool)
	for _, link := range links {
		validList[link.IfName] = true

		// Find the next ipv4, ipv6 uplink addresses to be used by ITRs.
		for _, addrInfo := range link.AddrInfoList {
			if ipv6Found && ipv4Found {
				break
			}
			// ipv6 case
			if (addrInfo.Addr.To4() == nil) && (ipv6Found == false) {
				// This address is ipv6
				ipv6Addr = addrInfo.Addr
				ipv6Found = true
				if debug {
					log.Printf("HandleDeviceNetworkChange: Picked ipv6 source address %s\n",
						ipv6Addr)
				}
			} else if ipv4Found == false {
				// This address is ipv4
				ipv4Addr = addrInfo.Addr
				ipv4Found = true
				if debug {
					log.Printf("HandleDeviceNetworkChange: Picked ipv4 source address %s\n",
						ipv6Addr)
				}
			}
		}

		// Check if this uplink present in the current etr table.
		// If not, start capturing packets from this new uplink.
		_, ok := EtrTable.EtrTable[link.IfName]
		if ok == false {
			//var ring *pfring.Ring = nil
			var handle *afpacket.TPacket
			var fd int = -1

			// Send a message on channel to kill the ETR thread when required.
			killChannel := make(chan bool, 1)

			// Create new ETR thread
			if EtrTable.EphPort != -1 {
				//ring, fd = StartEtrNat(EtrTable.EphPort, link.IfName)
				handle, fd = StartEtrNat(EtrTable.EphPort, link.IfName, killChannel)
				log.Printf("XXXXX Creating ETR thread for UP link %s\n", link.IfName)
				if debug {
					log.Printf("HandleDeviceNetworkChange: Creating ETR thread "+
						"for UP link %s\n", link.IfName)
				}
			}
			EtrTable.EtrTable[link.IfName] = &dptypes.EtrRunStatus{
				IfName: link.IfName,
				//Ring:   ring,
				Handle:      handle,
				RingFD:      fd,
				KillChannel: killChannel,
			}
			if debug {
				log.Printf("HandleDeviceNetworkChange: Creating ETR thread for UP link %s\n",
					link.IfName)
			}
		}
	}

	// find the interfaces to be deleted
	for key, link := range EtrTable.EtrTable {
		if _, ok := validList[key]; ok == false {
			link.KillChannel <- true
			syscall.Close(link.RingFD)
			//link.Ring.Disable()
			//link.Ring.Close()
			link.Handle.Close()
			delete(EtrTable.EtrTable, key)
		}
	}

	log.Printf("HandleDeviceNetworkChange: Setting Uplink v4 addr %s, v6 addr %s\n",
		ipv4Addr, ipv6Addr)
	fib.SetUplinkAddrs(ipv4Addr, ipv6Addr)
}

// Handle ETR's ephemeral port message from lispers.net
func HandleEtrEphPort(ephPort int) {
	// Check if the ephemeral port has changed
	if ephPort == EtrTable.EphPort {
		return
	}
	EtrTable.EphPort = ephPort

	// Destroy all old threads and create new ETR threads
	for ifName, link := range EtrTable.EtrTable {
		//if (link.Ring == nil) && (link.RingFD == -1) {
		if (link.Handle == nil) && (link.RingFD == -1) {
			log.Printf("XXXXX Creating ETR thread for UP link %s\n", link.IfName)
			if debug {
				log.Printf("HandleEtrEphPort: Creating ETR thread for UP link %s\n",
					link.IfName)
			}
			//ring, fd := StartEtrNat(EtrTable.EphPort, link.IfName)
			//ling.Ring = ring
			handle, fd := StartEtrNat(EtrTable.EphPort, link.IfName, link.KillChannel)
			link.Handle = handle
			link.RingFD = fd
			//return
			continue
		}

		// Remove the old BPF filter
		//link.Ring.RemoveBPFFilter()

		// Add the new BPF filter with new eph port match
		filter := fmt.Sprintf(etrNatPortMatch, ephPort)
		//link.Ring.SetBPFFilter(filter)

		// For AF_PACKET sockers old filter is replaced with new one.
		ins, err := pcap.CompileBPFFilter(layers.LinkTypeEthernet,
			1600, filter)
		if err != nil {
			log.Printf("SetupEtrPktCapture: Compiling BPF filter %s failed: %s\n", filter, err)
		} else {
			raw_ins := *(*[]bpf.RawInstruction)(unsafe.Pointer(&ins))
			err = link.Handle.SetBPF(raw_ins)
			if err != nil {
				log.Printf("SetupEtrPktCapture: Setting BPF filter %s failed: %s\n", filter, err)
			}
		}
		//link.Handle.SetBPFFilter(filter)

		log.Printf("HandleEtrEphPort: Changed ephemeral port BPF match for ETR %s\n",
			ifName)
	}
}

//func StartEtrNat(ephPort int, upLink string) (*pfring.Ring, int) {
func StartEtrNat(ephPort int,
	upLink string,
	killChannel chan bool) (*afpacket.TPacket, int) {

	//ring := SetupEtrPktCapture(ephPort, upLink)
	//if ring == nil {
	if debug {
		log.Printf("StartEtrNat: ETR thread (%s) with ephemeral port %d\n",
			upLink, ephPort)
	}
	handle := SetupEtrPktCapture(ephPort, upLink)
	if handle == nil {
		log.Fatal("StartEtrNat: Unable to create ETR packet capture.\n")
		return nil, -1
	}

	fd, err := syscall.Socket(syscall.AF_INET6, syscall.SOCK_RAW, syscall.IPPROTO_RAW)
	if err != nil {
		//ring.Disable()
		//ring.Close()
		handle.Close()
		log.Fatal(
			"StartEtrNat:Creating second ETR raw socket for packet injection failed: %s\n",
			err)
		return nil, -1
	}
	//go ProcessCapturedPkts(fd, ring)
	go ProcessCapturedPkts(fd, handle, killChannel)

	//return ring, fd
	return handle, fd
}

func verifyAndInject(fd6 int,
	buf []byte, n int,
	decapKeys *dptypes.DecapKeys) bool {

	// Check if packet is too small to include a full size 8 byte lisp header
	if n < dptypes.LISPHEADERLEN {
		fib.AddDecapStatistics("lisp-header-error", 1)
		return false
	}

	//var pktEid net.IP
	iid := fib.GetLispIID(buf[0:8])
	if iid == uint32(0xFFFFFF) {
		return true
	}
	log.Println("XXXXX IID of packet is:", iid)
	packetOffset := dptypes.LISPHEADERLEN

	// offset of destination address inside ipv6 header
	//destAddrOffset := 24
	gcmOverhead := 0
	icvLen := 0

	keyId := fib.GetLispKeyId(buf[0:8])
	if keyId != 0 {
		if decapKeys == nil {
			log.Printf("verifyAndInject: Decap keys for this RLOC have not arrived yet\n")
			fib.AddDecapStatistics("no-decrypt-key", 1)
			return false
		}

		//destAddrOffset += aes.BlockSize
		//destAddrOffset += dptypes.GCMIVLENGTH
		//packetOffset += aes.BlockSize
		packetOffset += dptypes.GCMIVLENGTH

		// Compute and compare ICV of packet.
		// Zededa ITRs always pick keyId of 1.
		// We read the key id from lisp header for inter-op with lispers.net
		key := decapKeys.Keys[keyId-1]
		icvKey := key.IcvKey
		if icvKey == nil {
			log.Printf("verifyAndInject: ETR Key id %d had nil ICV key value\n", keyId)
			return false
		}
		icv := fib.ComputeICV(buf[0:n-dptypes.ICVLEN], icvKey)
		pktIcv := buf[n-dptypes.ICVLEN : n]

		if !bytes.Equal(icv, pktIcv) {
			log.Printf(
				"verifyAndInject: Pkt ICV %x and calculated ICV %x do not match.\n",
				pktIcv, icv)
			fib.AddDecapStatistics("ICV-error", 1)
			return false
		}
		log.Println("XXXXX ICVs match")

		// Decrypt the packet before sending out.
		// Read the IV from packet buffer.
		ivArray := buf[dptypes.LISPHEADERLEN:packetOffset]

		packet := buf[packetOffset : n-dptypes.ICVLEN]

		// The following check is not necessary with AES/GCM.
		// 16 byte boundary is not used while encrypting.
		/*
			cryptoLen := n - packetOffset - dptypes.ICVLEN
			if cryptoLen%16 != 0 {
				// AES encrypted packet should have a lenght that is multiple of 16
				// aes.BlockSize is 16
				log.Printf("verifyAndInject: Invalid Crypto packet length %d\n", cryptoLen)
				return false
			}
		*/

		if len(decapKeys.Keys) == 0 {
			log.Printf(
				"verifyAndInject: ETR has not received decap keys from lispers.net yet\n")
			return false
		}

		block := key.DecBlock
		//mode := cipher.NewCBCDecrypter(block, ivArray)
		aesGcm, err := cipher.NewGCM(block)
		if err != nil {
			log.Printf("VerifyAndInject: GCM cipher creation failed: %s\n", err)
			return false
		}
		if debug {
			log.Printf("verifyAndInject: LISP %s, IV %s, Cipher %s, ICV %s\n",
				fib.PrintHexBytes(buf[:8]),
				fib.PrintHexBytes(ivArray),
				fib.PrintHexBytes(packet),
				fib.PrintHexBytes(pktIcv))
		}
		//mode.CryptBlocks(packet, packet)
		_, err = aesGcm.Open(packet[:0], ivArray, packet, nil)
		if err != nil {
			log.Printf("verifyAndInject: Packet decryption failed: %s\n", err)
			return false
		}
		gcmOverhead = aesGcm.Overhead()
		icvLen = dptypes.ICVLEN
	}

	// Zededa's use case only have ipv6 EIDs. Check if the version
	// of inner packet is ipv6. Else drop the packet and increment
	// error count.
	var msb byte = buf[packetOffset]
	version := msb >> 4
	if version != 6 {
		fib.AddDecapStatistics("bad-inner-version", 1)
		return false
	}

	// 24 is the offset of destination ipv6 address in ipv6 header
	destAddrOffset := packetOffset + 24
	packetEnd := n - gcmOverhead - icvLen
	var destAddr [16]byte
	for i, _ := range destAddr {
		// offset is lisp hdr size + start offset of ip addresses in v6 hdr
		//destAddr[i] = buf[dptypes.LISPHEADERLEN+destAddrOffset+i]
		destAddr[i] = buf[destAddrOffset+i]
		//pktEid[i] = destAddr[i]
	}

	err := syscall.Sendto(fd6, buf[packetOffset:packetEnd], 0, &syscall.SockaddrInet6{
		Port:   0,
		ZoneId: 0,
		Addr:   destAddr,
	})
	if err != nil {
		log.Printf("verifyAndInject: Failed injecting ETR packet: %s.\n", err)
		return false
	}
	fib.AddDecapStatistics("good-packets", 1)
	return true
}

//func SetupEtrPktCapture(ephemeralPort int, upLink string) *pfring.Ring {
//	ring, err := pfring.NewRing(upLink, 65536, pfring.FlagPromisc)
func SetupEtrPktCapture(ephemeralPort int, upLink string) *afpacket.TPacket {
	const (
		// Memory map buffer size in mega bytes
		mmapBufSize int = 24

		// set interface in promiscous mode
		promisc bool = true
	)

	if debug {
		log.Printf("SetupEtrPktCapture: Setup ETR NAT capture on interface %s, "+
			"ephemeral port %d\n", upLink, ephemeralPort)
	}

	frameSize := 65536
	blockSize := frameSize * 128
	numBlocks := 10

	tPacket, err := afpacket.NewTPacket(
		afpacket.OptInterface(upLink),
		afpacket.OptFrameSize(frameSize),
		afpacket.OptBlockSize(blockSize),
		afpacket.OptNumBlocks(numBlocks),
		afpacket.OptPollTimeout(5*time.Second),
		afpacket.OptBlockTimeout(1*time.Millisecond),
		afpacket.OptTPacketVersion(afpacket.TPacketVersion3))
	if err != nil {
		//log.Printf("ETR packet capture on interface %s failed: %s\n",
		//	upLink, err)
		log.Printf("SetupEtrPktCapture: Error: "+
			"Opening afpacket interface %s: %s\n", upLink, err)
		return nil
	}

	/*
		// We only read packets from this interface
		ring.SetDirection(pfring.ReceiveOnly)
		ring.SetSocketMode(pfring.ReadOnly)
	*/

	// Set filter for UDP, source port = 4341, destination port = given ephemeral
	filter := fmt.Sprintf(etrNatPortMatch, ephemeralPort)
	//ring.SetBPFFilter(filter)

	ins, err := pcap.CompileBPFFilter(layers.LinkTypeEthernet,
		1600, filter)
	if err != nil {
		log.Printf("SetupEtrPktCapture: Compiling BPF filter %s failed: %s\n", filter, err)
	} else {
		raw_ins := *(*[]bpf.RawInstruction)(unsafe.Pointer(&ins))
		err = tPacket.SetBPF(raw_ins)
		if err != nil {
			log.Printf("SetupEtrPktCapture: Setting BPF filter %s failed: %s\n", filter, err)
		}
	}
	//tPacket.SetBPFFilter(filter)

	/*
		ring.SetPollWatermark(1)
		// set a poll duration of 1 hour
		ring.SetPollDuration(60 * 60 * 1000)

		err = ring.Enable()
		if err != nil {
			log.Printf("SetupEtrPktCapture: Enabling pfring on interface %s failed: %s\n",
				upLink, err)
			return nil
		}
		return ring
	*/

	return tPacket
}

func ProcessETRPkts(fd6 int, serverConn *net.UDPConn) bool {
	// start processing packets. This loop should never end.
	buf := make([]byte, 65536)
	if debug {
		log.Printf("Started processing captured packets in ETR\n")
	}

	for {
		n, saddr, err := serverConn.ReadFromUDP(buf)
		log.Println("XXXXX Received", n, "bytes in ETR")
		if err != nil {
			log.Fatal("ProcessETRPkts: Fatal error during ETR processing\n")
			return false
		}
		decapKeys := fib.LookupDecapKeys(saddr.IP)
		ok := verifyAndInject(fd6, buf, n, decapKeys)
		if ok == false {
			log.Printf("Failed injecting ETR packet from port 4341\n")
		}
	}
}

//func ProcessCapturedPkts(fd6 int, ring *pfring.Ring) {
func ProcessCapturedPkts(fd6 int,
	handle *afpacket.TPacket,
	killChannel chan bool) {

	var pktBuf [65536]byte
	if debug {
		log.Printf("Started processing captured packets in ETR\n")
	}

	for {
		//ci, err := ring.ReadPacketDataTo(pktBuf[:])
		ci, err := handle.ReadPacketDataTo(pktBuf[:])
		if err != nil {
			select {
			case <-killChannel:
				log.Printf("ProcessCapturedPkts: Error capturing packets: %s\n", err)
				log.Printf(
					"ProcessCapturedPkts: It could be the handle closure leading to this.\n")
				return
			default:
				continue
			}
		}
		capLen := ci.CaptureLength
		log.Printf("XXXXX Captured ETR packet of length %d\n", capLen)
		packet := gopacket.NewPacket(
			pktBuf[:capLen],
			layers.LinkTypeEthernet,
			gopacket.DecodeOptions{Lazy: false, NoCopy: true})

		appLayer := packet.ApplicationLayer()
		if appLayer == nil {
			continue
		}
		payload := appLayer.Payload()
		if payload == nil {
			continue
		}

		var srcIP net.IP
		if ipLayer := packet.Layer(layers.LayerTypeIPv4); ipLayer != nil {
			// ipv4 underlay
			ipHdr := ipLayer.(*layers.IPv4)
			// validate outer header checksum
			csum := computeChecksum(pktBuf[dptypes.ETHHEADERLEN:dptypes.IP4HEADERLEN])
			if csum != 0xFFFF {
				fib.AddDecapStatistics("checksum-error", 1)
			}

			srcIP = ipHdr.SrcIP
		} else if ip6Layer := packet.Layer(layers.LayerTypeIPv6); ip6Layer != nil {
			// ipv6 underlay
			ip6Hdr := ip6Layer.(*layers.IPv6)
			srcIP = ip6Hdr.SrcIP
		} else {
			// We do not need this packet
			fib.AddDecapStatistics("outer-header-error", 1)
			return
		}

		decapKeys := fib.LookupDecapKeys(srcIP)

		ok := verifyAndInject(fd6, payload, len(payload), decapKeys)
		if ok == false {
			log.Printf("ProcessCapturedPkts: ETR Failed injecting packet from RLOC %s\n",
				srcIP.String())
		}
	}
}

func computeChecksum(buf []byte) uint32 {
	if (len(buf) % 2) != 0 { 
		fmt.Printf("Invalid length: %v\n", len(buf))
		return 0
	}   
	var csum uint32 = 0 
	var segment uint32

	for i := 0; i < len(buf); i += 2 { 
		segment = uint32(buf[i]) << 8
		segment += uint32(buf[i + 1]) 
		csum += segment
	}   

	remainder := csum >> 16
	csum += remainder

	return csum & 0xffff
}

