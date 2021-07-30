//
//  PacketTunnelProvider.swift
//  NEPacketTunnelVPNDemoTunnel
//
//  Created by Hench on 11/17/20.
//  Copyright © 2020 Hench. All rights reserved.
//
import UIKit
import NetworkExtension

extension Data {
    struct HexEncodingOptions: OptionSet {
        let rawValue: Int
        static let upperCase = HexEncodingOptions(rawValue: 1 << 0)
    }

    func hexEncodedString(options: HexEncodingOptions = []) -> String {
        let format = options.contains(.upperCase) ? "%02hhX" : "%02hhx"
        return map { String(format: format, $0) }.joined()
    }
}

class PacketTunnelProvider: NEPacketTunnelProvider ,NSFilePresenter  {
    //var connection: NWTCPConnection? = nil
    private var pendingCompletion: ((Error?) -> Void)?
    var conf = [String: AnyObject]()
   // var connected = 0
    open var remoteHost: String?
    var private_ip = ""
    var private_netmask = ""
    //var swift_log_file : URL!;
    var g_outputStream :OutputStream!;
    
    var g_private_ip = "";
    var serverAddress = "www.tinyvpn.xyz"
    var serverPort = "14433"
    
    var presentedItemURL: URL?
    var presentedItemOperationQueue: OperationQueue = OperationQueue.main
    
    override init() {
        let text = "NEPacketTunnel.Provider: init"
        NSLog("tvpn " + text)
        super.init()

    }

    func writeToFile(todayTraffic: NSInteger, monthTraffic: NSInteger, dayLimit: NSInteger, monthLimit: NSInteger)
    {
        let text = String(todayTraffic) + ";" + String(monthTraffic) + ";" + String(dayLimit) + ";" + String(monthLimit) //just a text
        let coordinator = NSFileCoordinator(filePresenter: self)
        coordinator.coordinate(writingItemAt: presentedItemURL!, options: .forReplacing, error: nil) { url in
            do {
                try text.write(to: url, atomically: false, encoding: .utf8)
            }
            catch { print("writing failed") }
            NSLog("tvpn writeToFile:\(text)")
        }
    }
    //func presentedItemDidChange(){}
    
    // read from tun, send to socket
    func tunToSocket() {
        self.packetFlow.readPackets  {  packets, protocols in  // packets: [Data], protocols: [NSNumber]) in
            var text = "readPackets:" + String(packets.count)
            //NSLog("tvpn " + text)
            for packet in packets {
                let service = NetworkService()
                var packet2 = packet
                let packet_len = packet2.count
               
                //let format = NumberFormatter()
                //format.numberStyle = .decimal
                //let dd=format.string(from: packet_len as NSNumber) ?? ""

                packet2.withUnsafeMutableBytes({(p: UnsafeMutablePointer<UInt8>) -> Void in
                    service.send_packet(p, len: packet_len)

                })
            }
            // Recursive to keep reading
            self.tunToSocket()
            
            //self.writeToFile()
        }
    }
    func setupPacketTunnelNetworkSettings() {
        let tunnelNetworkSettings = NEPacketTunnelNetworkSettings(tunnelRemoteAddress: self.protocolConfiguration.serverAddress!)
        tunnelNetworkSettings.ipv4Settings = NEIPv4Settings(addresses: [g_private_ip], subnetMasks: [conf["subnet"] as! String]);
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
        let text = "setTunnelNetworkSettings:" + (g_private_ip) + "," + (conf["subnet"] as! String) + "," + (conf["dns"] as! String)
            + "," + (conf["log_file"] as! String) + "," + (conf["uuid"] as! String) + "," + (conf["premium"] as! String)
            + "," + (conf["username"] as! String) + "," + (conf["password"] as! String)
        NSLog("tvpn " + text)

        self.setTunnelNetworkSettings(tunnelNetworkSettings) {(error: Error?) -> Void in
            // socket to tun
            let service = NetworkService()
            let dispatchQueue = DispatchQueue(label: "QueueIdentification", qos: .background)
            let observer = UnsafeMutableRawPointer(Unmanaged.passUnretained(self).toOpaque())
            //typealias CFunction = @convention(c) (UnsafeMutablePointer<UInt8>, Int, UnsafeMutablePointer<Void>) -> Int
            dispatchQueue.async {
                service.set_socket_recv(
                    {
                        // (p: UnsafeMutablePointer<UInt8>, len:Int, observer2) -> Int in
                        (p, len, observer2) in
                        let mySelf = Unmanaged<PacketTunnelProvider>.fromOpaque(observer2!).takeUnretainedValue()
                        DispatchQueue.main.async{
                            let a = UnsafeMutableBufferPointer(start: p, count: len)
                            let b=Array(a)
                            let data = Data(bytes: b, count: len)

                            mySelf.packetFlow.writePackets([data], withProtocols: [ AF_INET as NSNumber ])
                            //NSLog("tvpn send to tun: \(len), \(data.hexEncodedString())")
                        }
                        return
                    },
                    withTarget: observer)
            }
            var text = "set_socket_recv ok"
            NSLog("tvpn " + text)
            
            // tun to socket
            self.tunToSocket()
            text = "tunToSocket ok"
            NSLog("tvpn " + text)
            
            self.pendingCompletion?(error)
            self.pendingCompletion = nil
            
            text = "tunToSocket ok2."
            NSLog("tvpn " + text)
        }
    }
    func connect_server() {
        let service = NetworkService()
        let dispatchQueue = DispatchQueue(label: "QueueIdentification", qos: .background)
        let observer = UnsafeMutableRawPointer(Unmanaged.passUnretained(self).toOpaque())

        service.init_vpn(conf["log_file"] as? String)
        var premium = conf["premium"] as? String
        service.start_vpn(conf["username"] as? String, pwd: conf["password"] as? String, device_id: conf["uuid"] as? String,
            premium: Int(premium ?? "0")!, country_code: conf["country"] as? String,
            stop_call: { (status_ret:Int, observer) in
                let mySelf = Unmanaged<PacketTunnelProvider>.fromOpaque(observer!).takeUnretainedValue()
                DispatchQueue.main.async{
                    //0 start vpn succfully, 1 stop vpn
  /*                  if status_ret == 1 {
                        mySelf.vpnManager.connection.stopVPNTunnel()
                        
                        mySelf.txtSwitch.text = "VPN Off"
                        mySelf.isRun = 0
                        print("vpn off")
                        mySelf.swiVpn.isOn = false
                    }
 */
                    mySelf.writeToFile(todayTraffic:-1, monthTraffic:-1, dayLimit:-1, monthLimit:-1)
                    let text = "stop call." + String(status_ret)
                    NSLog("tvpn " + text)
                    var r = NEProviderStopReason(rawValue: 1)
                    mySelf.cancelTunnelWithError(nil)
                }
            },
            traffic_call: { (todayTraffic:Int,monthTraffic:Int,dayLimit:Int,monthLimit:Int,observer) in
                let mySelf = Unmanaged<PacketTunnelProvider>.fromOpaque(observer!).takeUnretainedValue()
                DispatchQueue.main.async{
/*                    if dayLimit != 0 {
                        mySelf.g_day_limit = dayLimit
                    }
                    if monthLimit != 0 {
                        mySelf.g_month_limit = monthLimit
                    }
                    mySelf.show_traffic(d: todayTraffic, m: monthTraffic)
 */
                    let text = "traffic call." + String(dayLimit) + "," + String(monthLimit) + "," + String(todayTraffic) + "," + String(monthTraffic)
                    NSLog("tvpn " + text)
                    mySelf.writeToFile(todayTraffic:todayTraffic, monthTraffic:monthTraffic, dayLimit:dayLimit, monthLimit:monthLimit)
                }
            },
            get_ip_call: { (private_ip:Int, server_ip:Int, server_port:Int, observer) in
                let mySelf = Unmanaged<PacketTunnelProvider>.fromOpaque(observer!).takeUnretainedValue()
                DispatchQueue.main.async{

                    //g_private_ip = (private_ip>>24) + "." + (private_ip>>16 & 0xff) +"." + (private_ip>>8 & 0xff) +"." + (private_ip & 0xff)
                    mySelf.g_private_ip = String(format:"%d.%d.%d.%d", (private_ip>>24) , (private_ip>>16 & 0xff) , (private_ip>>8 & 0xff), (private_ip & 0xff))
                    mySelf.serverAddress = String(format:"%d.%d.%d.%d", (server_ip>>24) , (server_ip>>16 & 0xff) , (server_ip>>8 & 0xff), (server_ip & 0xff))
                    mySelf.serverPort = String(server_port)
                
                    print("g_private_ip",mySelf.g_private_ip)

                    var text = "g_private_ip" + mySelf.g_private_ip + ",server:" + mySelf.serverAddress + ":" + mySelf.serverPort + "\n"
                    mySelf.g_outputStream.write(text, maxLength: text.count)
                    // open tun......
                    mySelf.setupPacketTunnelNetworkSettings()
                    text = "initVPNTunnelProviderManager ok." + mySelf.g_private_ip
                    NSLog("tvpn " + text)
                    
                }
            },
            withTarget: observer)
    }
    override func startTunnel(options: [String : NSObject]?, completionHandler: @escaping (Error?) -> Void) {
        // Add code here to start the process of connecting the tunnel.

        let file = "plog.txt" //this is the file. we will write to and read from it
        if let dir = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first {
            var swift_log_file :URL!;
            swift_log_file = dir.appendingPathComponent(file)

            g_outputStream = OutputStream(url: swift_log_file, append: true)
            g_outputStream.open()

        }
        //do {try text.write(to: self.swift_log_file, atomically: false, encoding: .utf8)}catch{}
        var text = "PacketTunnelProviderTcp init"
        g_outputStream.write(text, maxLength: text.count)
        NSLog("tvpn " + text)

        conf = (self.protocolConfiguration as! NETunnelProviderProtocol).providerConfiguration! as [String : AnyObject]
        self.pendingCompletion = completionHandler
        
        self.setTunnelNetworkSettings(nil) { (error: Error?) -> Void in
            if let error = error {
                print(error)
                NSLog("tvpn " + error.localizedDescription)
            }
            
            self.connect_server()
            
            text = "setupPacketTunnelNetworkSettings ok"
            NSLog("tvpn " + text)
            
        }
        text = "start tunnel ok"
        NSLog("tvpn " + text)
        

        let traffic_file = "user_traffic"
        let dir = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: "group.com.TinyVpn")!
        presentedItemURL = dir.appendingPathComponent(traffic_file)
        
        
    }
    
    override func stopTunnel(with reason: NEProviderStopReason, completionHandler: @escaping () -> Void) {
        // Add code here to start the process of stopping the tunnel.
           //     connection?.cancel()
        completionHandler()
    }
    
    override func handleAppMessage(_ messageData: Data, completionHandler: ((Data?) -> Void)?) {
        // Add code here to handle the message.
        if let handler = completionHandler {
            handler(messageData)
        }
    }
    
    override func sleep(completionHandler: @escaping () -> Void) {
        // Add code here to get ready to sleep.
        completionHandler()
    }
    
    override func wake() {
        // Add code here to wake up.
    }

}
