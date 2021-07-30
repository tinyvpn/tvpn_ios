//
//  PacketTunnelProviderTcp.swift
//  NEPacketTunnelVPNDemoTunnel
//
//  Created by imac on 2020/4/17.
//  Copyright © 2020年 lxd. All rights reserved.
//
import UIKit
import NetworkExtension
//import TinyVpn


class PacketTunnelProviderTcp: NEPacketTunnelProvider {
    var connection: NWTCPConnection? = nil
    var conf = [String: AnyObject]()
    var connected = 0
    open var remoteHost: String?
    var private_ip = ""
    var private_netmask = ""
    var swift_log_file : URL!;
    var g_outputStream :OutputStream!;
    
    override init() {
        NSLog("NEPacketTunnel.Provider: init")
        let file = "plog.txt" //this is the file. we will write to and read from it
        if let dir = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first {
            swift_log_file = dir.appendingPathComponent(file)
        }
        g_outputStream = OutputStream(url: swift_log_file, append: true)
        g_outputStream.open()
        //do {try text.write(to: self.swift_log_file, atomically: false, encoding: .utf8)}catch{}
        let text = "PacketTunnelProviderTcp init"
        g_outputStream.write(text, maxLength: text.count)
        super.init()

    }
    // read from tun, send to socket
    func tunToSocket() {
        if (connected == 0) {
            return
        }
        self.packetFlow.readPackets { (packets: [Data], protocols: [NSNumber]) in
            for packet in packets {
                // send to socket
                let service = NetworkService()
                //service.send_packet(packet.map { String(format: "%02x", $0) }.joined())
                var packet2 = packet
                let packet_len = packet2.count
                
                let format = NumberFormatter()
                format.numberStyle = .decimal
                let dd=format.string(from: packet_len as NSNumber) ?? ""
                let text = "recv from tun:" + dd
                do { try text.write(to: self.swift_log_file, atomically: false, encoding: .utf8)
                }catch{}
                packet2.withUnsafeMutableBytes({(p: UnsafeMutablePointer<UInt8>) -> Void in
                    service.send_packet(p, len: packet_len)
                })
                
 /*               self.connection?.write(packet, completionHandler: { (error: Error?) in
                    if let error = error {
                        print(error)
                        return
                    }
                })*/
            }
            // Recursive to keep reading
            self.tunToSocket()
        }
    }
    
    func setupPacketTunnelNetworkSettings() {
        let tunnelNetworkSettings = NEPacketTunnelNetworkSettings(tunnelRemoteAddress: self.protocolConfiguration.serverAddress!)
        tunnelNetworkSettings.ipv4Settings = NEIPv4Settings(addresses: [conf["ip"] as! String], subnetMasks: [conf["subnet"] as! String]);
        //(addresses: [private_ip], subnetMasks: [private_netmask])
        
        // Refers to NEIPv4Settings#includedRoutes or NEIPv4Settings#excludedRoutes,
        // which can be used as basic whitelist/blacklist routing.
        // This is default routing.
        tunnelNetworkSettings.ipv4Settings?.includedRoutes = [NEIPv4Route.default()]
        
        tunnelNetworkSettings.mtu = Int(conf["mtu"] as! String) as NSNumber?
        
        let dnsSettings = NEDNSSettings(servers: (conf["dns"] as! String).components(separatedBy: ","))
        // This overrides system DNS settings
        dnsSettings.matchDomains = [""]
        tunnelNetworkSettings.dnsSettings = dnsSettings
        
        let format = NumberFormatter()
        format.numberStyle = .decimal
        //let dd=format.string(from: packet_len as NSNumber) ?? ""
        let text = "setTunnelNetworkSettings:" + (conf["ip"] as! String) + "," + (conf["subnet"] as! String) + "," + (conf["dns"] as! String)
        do {try text.write(to: self.swift_log_file, atomically: false, encoding: .utf8)}catch{}
        
        self.setTunnelNetworkSettings(tunnelNetworkSettings) {(error: Error?) -> Void in }
    }
  
    override func startTunnel(options: [String : NSObject]?, completionHandler: @escaping (Error?) -> Void) {

        var text = "start tunnel.\n"
        do {try text.write(to: self.swift_log_file, atomically: false, encoding: .utf8) }catch{}
        
        conf = (self.protocolConfiguration as! NETunnelProviderProtocol).providerConfiguration! as [String : AnyObject]
        
        self.setTunnelNetworkSettings(nil) { (error: Error?) -> Void in
            if let error = error {
                print(error)
            }
            
            self.setupPacketTunnelNetworkSettings()
        }
        text = "setupPacketTunnelNetworkSettings ok"
        do {try text.write(to: self.swift_log_file, atomically: false, encoding: .utf8)}catch{}
        
        // socket to tun
        let service = NetworkService()
        let dispatchQueue = DispatchQueue(label: "QueueIdentification", qos: .background)
        let observer = UnsafeMutableRawPointer(Unmanaged.passUnretained(self).toOpaque())
        //typealias CFunction = @convention(c) (UnsafeMutablePointer<UInt8>, Int, UnsafeMutablePointer<Void>) -> Int
        dispatchQueue.async {
            service.set_socket_recv(
                { //(p: UnsafeMutablePointer<UInt8>, len:Int, observer2) -> Void in
                    (p, len, observer2) in
                    let mySelf = Unmanaged<PacketTunnelProviderTcp>.fromOpaque(observer2!).takeUnretainedValue()
                    DispatchQueue.main.async{
                        let ptr = UnsafePointer(p)
                        let data = Data(bytes: [ptr], count: len)
                        
                        let format = NumberFormatter()
                        format.numberStyle = .decimal
                        let dd=format.string(from: len as NSNumber) ?? ""
                        let text = "send to tun:" + dd
                        do {try text.write(to: mySelf.swift_log_file, atomically: false, encoding: .utf8)}catch{}
                        mySelf.packetFlow.writePackets([data], withProtocols: [NSNumber](repeating: AF_INET as NSNumber, count: len))
                    }
                    return
                },
                withTarget: observer)
        }
        text = "set_socket_recv ok"
        do { try text.write(to: self.swift_log_file, atomically: false, encoding: .utf8) }catch{}
        
        // tun to socket
        self.tunToSocket()
        text = "tunToSocket ok"
        do { try text.write(to: self.swift_log_file, atomically: false, encoding: .utf8)}catch{}
    }
    
    override func stopTunnel(with reason: NEProviderStopReason, completionHandler: @escaping () -> Void) {
        connection?.cancel()
        super.stopTunnel(with: reason, completionHandler: completionHandler)
    }
    
    override func handleAppMessage(_ messageData: Data, completionHandler: ((Data?) -> Void)? = nil) {
        if let handler = completionHandler {
            handler(messageData)
        }
    }
    
    override func sleep(completionHandler: @escaping () -> Void) {
        completionHandler()
    }
    
    override func wake() {
    }
    
    // Handle changes to the tunnel connection state. open
    override func observeValue(forKeyPath keyPath: String?, of object: Any?, change: [NSKeyValueChangeKey : Any]?, context: UnsafeMutableRawPointer?) {
        guard keyPath == "state" && context?.assumingMemoryBound(to: Optional<NWTCPConnection>.self).pointee == connection
            else {
                super.observeValue(forKeyPath: keyPath, of: object, change: change, context: context)
                return
                
        }
        print("Tunnel connection state changed to \(connection!.state)")
        switch connection!.state {
        case .connected:
//            if let remoteAddress = self.connection!.remoteAddress as? NWHostEndpoint {
  //              remoteHost = remoteAddress.hostname
    //        }
            // Start reading messages from the tunnel connection.
            //readNextPacket()
            //self.setupPacketTunnelNetworkSettings()
            let text = "tunnel connected"
            do {
                try text.write(to: self.swift_log_file, atomically: false, encoding: .utf8)
            }catch{}
            break;
        case .disconnected:
            print("disconnected")
            let text = "tunnel disconnected"
            do {
                try text.write(to: self.swift_log_file, atomically: false, encoding: .utf8)
            }catch{}
            break;
            //closeTunnelWithError(connection!.error)
        case .cancelled:
            print("cancelled")
            let text = "tunnel cancelled"
            do {
                try text.write(to: self.swift_log_file, atomically: false, encoding: .utf8)
            }catch{}
            connection!.removeObserver(self, forKeyPath: "state", context: &connection);
            connection = nil
        //delegate?.tunnelDidClose(self)
        default: break
            
        }
        
    }
}
