//
//  ViewController.swift
//  TinyVpn
//
//  Created by Hench on 7/3/20.
//  Copyright © 2020 Hench. All rights reserved.
//

import UIKit
import NetworkExtension

import StoreKit
import Foundation

class ViewController: UIViewController ,UITextFieldDelegate,NSFilePresenter    {
    var vpnManager: NETunnelProviderManager = NETunnelProviderManager()

    @IBOutlet weak var swiVpn: UISwitch!
    @IBOutlet weak var btnLogin: UIButton!
    @IBOutlet weak var txtUserName: UITextField!
    @IBOutlet weak var txtPassword: UITextField!
    @IBOutlet weak var txtSwitch: UILabel!
    @IBOutlet weak var txtToday: UILabel!
    @IBOutlet weak var txtMonth: UILabel!
    @IBOutlet weak var txtUserStatus: UILabel!
    //@IBOutlet weak var vpnView: UIView!
    var vpnInfoView: UIView!
    
    let tunnelBundleId = "com.TinyVpn.NEPacketTunnelVPNDemoTunnel"
    var serverAddress = "192.168.1.1"; //"www.tinyvpn.xyz"
    var serverPort = "14433"
    let mtu = "16000"
    var g_private_ip = "126.24.0.1"
    let subnet = "255.255.0.0"
    let dns = "8.8.8.8"
    var private_ip = 0;
    
    let kScreenHeight = UIScreen.main.bounds.size.height // mainScreen].bounds.size.height
    let kRateHeight = UIScreen.main.bounds.size.height/667
    
    var connectStatus = 0;  // 0 off, 1 connecting , 2 connected
    var isOn = 0;
    //var isRun = 0;
    var g_premium = 0;
    var out_of_quota = 0;
    var g_day_limit = 0;
    var g_month_limit = 0;
    var g_logfile = "";
    var g_swift_log_file :URL!;
    var g_outputStream :OutputStream!;
    var g_country_code = ""
    var g_uuid = ""

    var presentedItemURL: URL?
    var presentedItemOperationQueue: OperationQueue = OperationQueue.main
    
    var angle = 0
    var vpnView = UIView()
    var connectButton = UIButton()
    var roundLineView = UIImageView()
    var connectImageView = UIImageView()
    var roundConnectedView = UIImageView()
    
    /// Data model used by all BaseViewController subclasses.
    var data = [Section]()
  /*  fileprivate lazy var products: Products = {
        let identifier = ViewControllerIdentifiers.products
        guard let controller = storyboard?.instantiateViewController(withIdentifier: identifier) as? Products
            else { print("unableToInstantiateProducts") }
        return controller
    }() */
    
    func presentedItemDidChange() { // posted on changed existed file only
        readFromFile()
    }
    private func readFromFile()
    {
        let coordinator = NSFileCoordinator(filePresenter: self)
        coordinator.coordinate(readingItemAt: presentedItemURL!, options: [], error: nil) { url in
            if let text2 = try? String(contentsOf: url, encoding: .utf8) {
                NSLog("tvpn readFromFile:\(text2)"); // demo label in view for test
                let array : Array = text2.components(separatedBy: ";")
                if (array.count >= 2) {
                    if (Int(array[0])! < 0) {
                        self.vpnManager.connection.stopVPNTunnel()
                        self.txtSwitch.text = "VPN Off"
                        self.connectStatus = 0
                        self.stopConnectAnimation()
                        self.swiVpn.isOn = false
                        NSLog("tvpn socket disconnect. VPN Off.")
                    } else {
                        show_traffic(d: Int(array[0])!, m: Int(array[1])! )
                    }
                }
            } else {
                NSLog("tvpn readFromFile: no text");
                //just initial creation of file needed to observe following changes
                coordinator.coordinate(writingItemAt: presentedItemURL!, options: .forReplacing, error: nil) { url in
                    do {
                        try "".write(to: url, atomically: false, encoding: .utf8)
                    }
                    catch { print("writing failed") }
                }
            }
        }
    }
    
    func hardwareUUID() -> String?
    {
        return UIDevice.current.identifierForVendor!.uuidString;
    }
    
    private func initVPNTunnelProviderManager() {
        NETunnelProviderManager.loadAllFromPreferences { (savedManagers: [NETunnelProviderManager]?, error: Error?) in
            if let error = error {
                print(error)
            }
            if let savedManagers = savedManagers {
                if savedManagers.count > 0 {
                    self.vpnManager = savedManagers[0]
                }
            }

            self.vpnManager.loadFromPreferences(completionHandler: { (error:Error?) in
                if let error = error {
                    print(error)
                }

                let providerProtocol = NETunnelProviderProtocol()
                providerProtocol.providerBundleIdentifier = self.tunnelBundleId
                let format = NumberFormatter()
                format.numberStyle = .decimal

                providerProtocol.providerConfiguration = ["port": self.serverPort,
                                                          "server": self.serverAddress,
                                                          "ip": self.g_private_ip,
                                                          "subnet": self.subnet,
                                                          "mtu": self.mtu,
                                                          "dns": self.dns,
                                                          "username": self.txtUserName?.text,
                                                          "password": self.txtPassword?.text,
                                                          "log_file": self.g_logfile,
                                                          "uuid": self.g_uuid,
                                                          "country": self.g_country_code,
                                                          "premium": format.string(from: self.g_premium as NSNumber) ?? "0"
                ]
                providerProtocol.serverAddress = self.serverAddress
                //let text = "serverAddress" + self.serverAddress + ":" + self.serverPort + "\n"
                //self.g_outputStream.write(text, maxLength: text.count)
                self.vpnManager.protocolConfiguration = providerProtocol
                self.vpnManager.localizedDescription = "TinyVpn"
                self.vpnManager.isEnabled = true

                self.vpnManager.saveToPreferences(completionHandler: { (error:Error?) in
                    if let error = error {
                        print(error)
                    } else {
                        print("Save successfully\n")
                       // let text = "Save successfully" + self.serverAddress + "," + self.serverPort + "," + self.g_private_ip + "," + self.subnet + "," + self.mtu + "," + self.dns + "\n"
                       // self.g_outputStream.write(text, maxLength: text.count)
                        
                        self.vpnManager.loadFromPreferences(completionHandler: { (error:Error?) in
                            if let error = error {
                                print(error)
                            } else {
                                let text = "load twice successfully\n"
                                self.g_outputStream.write(text, maxLength: text.count)
                                
                                do {
                                    try self.vpnManager.connection.startVPNTunnel()
                                    let text = "startVPNTunnel ok.\n"
                                    self.g_outputStream.write(text, maxLength: text.count)
                                } catch {
                                    print(error)
                                    
                                    let text = error.localizedDescription
                                    self.g_outputStream.write(text, maxLength: text.count)
                                }
                                
 //                               self.txtSwitch.text = "VPN On"
 //                               self.stopConnectAnimation()
 //                               self.connectStatus = 2
 //                               self.swiVpn.isOn = true
                            }
                        })
                    }
                })
                self.VPNStatusDidChange(nil)
            })
        }
    }
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)

        // register for presentedItemDidChange work
        //NSFileCoordinator.addFilePresenter(self)
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)

        // unregister - required !!
        //NSFileCoordinator.removeFilePresenter(self)
    }
    override func viewDidLoad() {
        super.viewDidLoad()
        NSLog("start app.")
        // Do any additional setup after loading the view, typically from a nib.
        let defaults = UserDefaults.standard
        if let stringOne = defaults.string(forKey: "UserName") {
            self.txtUserName.text = stringOne
            print("username:", stringOne)
        }
        if let stringTwo = defaults.string(forKey: "Password") {
            self.txtPassword.text = stringTwo
            print("password:", stringTwo)
        }
        NotificationCenter.default.addObserver(self, selector: #selector(ViewController.VPNStatusDidChange(_:)), name: NSNotification.Name.NEVPNStatusDidChange, object: nil)

        let file = "vlog.txt" //this is the file. we will write to and read from it
        if let dir = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first {
            g_swift_log_file = dir.appendingPathComponent(file)
            //writing
            do {
                let documentsPath = NSSearchPathForDirectoriesInDomains(.documentDirectory, .userDomainMask, true)[0]
                g_logfile = documentsPath
                //try documentsPath.write(toFile: g_logfile+"/tvpn.log", atomically: false, encoding: .utf8)
                g_outputStream = OutputStream(url: g_swift_log_file, append: true)
                g_outputStream.open()
            }
            catch {/* error handling here */}
        }
        (UIApplication.shared.delegate as! AppDelegate).restrictRotation = .portrait
        
        let traffic_file = "user_traffic"
        let dir = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: "group.com.TinyVpn")!
        presentedItemURL = dir.appendingPathComponent(traffic_file)
        NSFileCoordinator.addFilePresenter(self)
        
        txtUserName.topAnchor.constraint(equalTo: self.view.safeAreaLayoutGuide.topAnchor, constant: 40).isActive = true
        txtPassword.topAnchor.constraint(equalTo: txtUserName.bottomAnchor, constant: 20).isActive = true
        createAndAddVPNButtonTo()
        //txtSwitch.translatesAutoresizingMaskIntoConstraints = false
        //txtSwitch.frame = CGRect(x: txtUserName.frame.minX, y: self.vpnView.frame.maxY, width: 70, height: 40)
        //txtSwitch.topAnchor.constraint(equalTo: vpnView.bottomAnchor).isActive = true
        //txtSwitch.centerXAnchor.constraint(equalTo: self.view.safe).isActive = true

        //txtToday.frame = CGRect(x: txtUserName.frame.minX, y: txtSwitch.frame.maxY, width: self.view.frame.width, height: 40)
        //txtMonth.frame = CGRect(x: txtUserName.frame.minX, y: txtToday.frame.maxY, width: self.view.frame.width, height: 40)
        let resourceFile = ProductIdentifiers()
        guard let identifiers = resourceFile.identifiers else {
            // Warn the user that the resource file could not be found.
            print(Messages.status, resourceFile.wasNotFound)
            return
        }
        
        if identifiers.isEmpty {
            // Warn the user that the resource file does not contain anything.
            print(Messages.status, resourceFile.isEmpty)
        }
        
        StoreManager.shared.delegate = self
        StoreObserver.shared.delegate = self
        // hench: need fix StoreManager, only fetch my two products
        StoreManager.shared.startProductRequest(with: identifiers)
        
   }
    private func createAndAddVPNButtonTo() {
        // 使用界面button，考虑删除vpnView
/*        let image: UIImage = UIImage(named: "connButton")!
        let button = UIButton(type: UIButton.ButtonType.custom)
        button.setImage(image, for: UIControl.State.normal)
        button.frame = CGRect(x: self.view.frame.midX - image.size.width / 2, y: txtPassword.frame.maxY, width: image.size.width, height: image.size.height)
        self.view.addSubview(button)
*/
        
        //let vpnView = UIView()
        vpnView.backgroundColor = UIColor.clear
        //vpnView.backgroundColor = UIColor.green
        self.view.addSubview(vpnView)
   
        let button = UIButton(type: UIButton.ButtonType.custom)
        button.titleLabel?.font = UIFont.systemFont(ofSize: 20.0 * kRateHeight)

        let image = UIImage(named: "connButton")
        //button.setBackgroundImage(image, for: UIControl.State.normal)
        //button.setBackgroundImage(image, for: UIControl.State.highlighted)
        button.setImage(image, for: UIControl.State.normal)
        button.setTitle("Connect", for: UIControl.State.normal)
        //button.setTitleColor(UIColor(red: 0x06, green: 0x77, blue: 0xDF, alpha: 1), for: UIControl.State.normal)
        button.setTitleColor(UIColor.green, for: UIControl.State.normal)
        button.titleEdgeInsets = UIEdgeInsets(top: 0, left: -230, bottom: 0, right: 0)
        button.addTarget(self, action: #selector(self.buttonConnect), for: .touchUpInside)
        vpnView.addSubview(button)
        
        print(image!.size.width)
        print(image!.size.height)

        vpnView.frame = CGRect(x: self.view.frame.midX - image!.size.width / 2, y: txtPassword.frame.maxY, width: image!.size.width, height: image!.size.height)
        button.frame = CGRect(x: 0, y: 0, width: image!.size.width, height: image!.size.height)
        connectButton = button

        let roundLine = UIImageView(image: UIImage(named: "round_line"))
        roundLine.isUserInteractionEnabled = false
        roundLine.alpha = 0
        vpnView.addSubview(roundLine)
        //roundLine.widthAnchor.constraint(equalToConstant: image!.size.width - 10).isActive = true
        //roundLine.heightAnchor.constraint(equalToConstant: image!.size.height - 10).isActive = true
        //roundLine.centerXAnchor.constraint(equalTo: connectButton.centerXAnchor).isActive = true
        //roundLine.centerYAnchor.constraint(equalTo: connectButton.centerYAnchor).isActive = true
        roundLine.frame = CGRect(x: 5, y: 5, width: image!.size.width - 10, height: image!.size.height - 10)
        roundLineView = roundLine
      
        connectImageView = UIImageView(image: UIImage(named: "round_point"))
        connectImageView.isUserInteractionEnabled = false
        connectImageView.alpha = 0
        vpnView.addSubview(connectImageView)
        connectImageView.frame = CGRect(x: 15, y: 15, width: 200, height: 200)
//        connectImageView.widthAnchor.constraint(equalToConstant: connectButton.frame.width).isActive = true
//        connectImageView.heightAnchor.constraint(equalToConstant: connectButton.frame.height).isActive = true
//        connectImageView.centerXAnchor.constraint(equalTo: connectButton.centerXAnchor).isActive = true
//        connectImageView.centerYAnchor.constraint(equalTo: connectButton.centerYAnchor).isActive = true

        let roundConnected = UIImageView(image: UIImage(named: "round_200"))
        roundConnected.isUserInteractionEnabled = false
        roundConnected.alpha = 1
        vpnView.addSubview(roundConnected)
        roundConnected.frame = connectImageView.frame
//        roundConnected.widthAnchor.constraint(equalToConstant: connectButton.frame.width).isActive = true
//        roundConnected.heightAnchor.constraint(equalToConstant: connectButton.frame.height).isActive = true
//        roundConnected.centerXAnchor.constraint(equalTo: connectButton.centerXAnchor).isActive = true
//        roundConnected.centerYAnchor.constraint(equalTo: connectButton.centerYAnchor).isActive = true
        roundConnectedView = roundConnected
        vpnView.bringSubviewToFront(connectButton)
/*     */
    }
    @objc func buttonConnect(sender : UIButton) {
        if isOn == 0 {
            isOn = 1
            print("switch on")
            //let service = NetworkService()
            let dispatchQueue = DispatchQueue(label: "QueueIdentification", qos: .background)
            //let observer = UnsafeMutableRawPointer(Unmanaged.passUnretained(self).toOpaque())
            let locale = Locale.current
            print("country code:", locale.regionCode! as String)
            //let country_code2 = locale.regionCode
            g_country_code = locale.regionCode! as String
            g_uuid = self.hardwareUUID()! as String
            let username = self.txtUserName?.text
            let password = self.txtPassword?.text
            //let log_file = self.g_logfile
            print("username:", username! as String)
            print("password:", password! as String)
            dispatchQueue.async {
                self.initVPNTunnelProviderManager()
                let text = "initVPNTunnelProviderManager ok.\n"
                self.g_outputStream.write(text, maxLength: text.count)
            }
            
            self.processConnectingAnimAndStatus()
        }else {
            isOn = 0
            self.vpnManager.connection.stopVPNTunnel()
            self.txtSwitch.text = "VPN Off"
            self.connectStatus = 0
            self.stopConnectAnimation()
            self.swiVpn.isOn = false
            print("switch off")
        }
    }
    
    @objc func VPNStatusDidChange(_ notification: Notification?) {
        print("VPN Status changed:")
        let status = self.vpnManager.connection.status
        switch status {
        case .connecting:
            print("Connecting...")
            //connectButton.setTitle("Disconnect", for: .normal)
            let text="connecting...\n"
            self.g_outputStream.write(text, maxLength: text.count)
            break
        case .connected:
            print("Connected...")
            let text="connected...\n"
            self.g_outputStream.write(text, maxLength: text.count)
            //connectButton.setTitle("Disconnect", for: .normal)
            self.txtSwitch.text = "VPN On"
            self.connectStatus = 2
            self.stopConnectAnimation()
            self.swiVpn.isOn = true
            
            break
        case .disconnecting:
            print("Disconnecting...")
            let text="disconnecting...\n"
            self.g_outputStream.write(text, maxLength: text.count)
            break
        case .disconnected:
            print("Disconnected...")
            let text="disconnected...\n"
            self.g_outputStream.write(text, maxLength: text.count)
            //connectButton.setTitle("Connect", for: .normal)
            break
        case .invalid:
            let text="invalid...\n"
            self.g_outputStream.write(text, maxLength: text.count)
            print("Invliad")
            break
        case .reasserting:
            print("Reasserting...")
            let text="reasserting...\n"
            self.g_outputStream.write(text, maxLength: text.count)
            break
        default:
            print("default")
            break
        }
    }
    
    func show_traffic(d : NSInteger, m : NSInteger) {  // 100 M
        let format = NumberFormatter()
        format.numberStyle = .decimal
        out_of_quota = 0;
        let dd = format.string(from: d as NSNumber) ?? ""
        let dl = format.string(from: g_day_limit as NSNumber) ?? ""
        if (g_premium < 2 && g_day_limit != 0 && d > g_day_limit) {
            txtToday.text = "Today: " + dd + " kB. No enough quota."
            //let service = NetworkService()
            //service.stop_vpn(1)
            self.vpnManager.connection.stopVPNTunnel()
            self.txtSwitch.text = "VPN Off"
            self.connectStatus = 0
            self.stopConnectAnimation()
            self.swiVpn.isOn = false
            print("out of quota")
            out_of_quota = 1;
        } else {
            if (g_premium < 2) {
                txtToday.text = "Today: " + dd + " kB / " + dl + " kB"
            } else {
                txtToday.text = "Today: " + dd + " kB"
            }
        }
        let mm = format.string(from: m as NSNumber) ?? ""
        let ml = format.string(from: g_month_limit as NSNumber) ?? ""
        if (g_premium >= 2 && g_month_limit != 0 && m > g_month_limit) {  // 10 G
            txtMonth.text = "This month: " + mm + " kB. No enough quota."
            //let service = NetworkService()
            //service.stop_vpn(1)
            self.vpnManager.connection.stopVPNTunnel()
            self.txtSwitch.text = "VPN Off"
            connectStatus = 0
            self.stopConnectAnimation()
            self.swiVpn.isOn = false
            print("out of quota.")
            out_of_quota = 1;
        } else {
            if (g_premium >= 2){
                txtMonth.text = "This month: " + mm + " kB / " + ml + " kB"
            } else {
                txtMonth.text = "This month: " + mm + " kB"
            }
        }
    }
    @IBAction func switch_on(_ sender: UISwitch) {
        if swiVpn.isOn {
            print("switch on")
            //let service = NetworkService()
            let dispatchQueue = DispatchQueue(label: "QueueIdentification", qos: .background)
            //let observer = UnsafeMutableRawPointer(Unmanaged.passUnretained(self).toOpaque())
            let locale = Locale.current
            print("country code:", locale.regionCode! as String)
            //let country_code2 = locale.regionCode
            g_country_code = locale.regionCode! as String
            g_uuid = self.hardwareUUID()! as String
            let username = self.txtUserName?.text
            let password = self.txtPassword?.text
            //let log_file = self.g_logfile
            print("username:", username! as String)
            print("password:", password! as String)
            dispatchQueue.async {
                //self.isRun = 1
                self.initVPNTunnelProviderManager()
                let text = "initVPNTunnelProviderManager ok.\n"
                self.g_outputStream.write(text, maxLength: text.count)
                //self.isRun = 0
            }
            
            self.processConnectingAnimAndStatus()
        }else {
            self.vpnManager.connection.stopVPNTunnel()
            self.txtSwitch.text = "VPN Off"
            self.connectStatus = 0
            self.stopConnectAnimation()
            self.swiVpn.isOn = false
            print("switch off")
        }
    }
    func processConnectingAnimAndStatus() {
        startConnectAnimation()
        connectButton.setTitle("Cancel", for: UIControl.State.normal)
    }
    func startConnectAnimation() {
        angle = 0
        roundConnectedView.alpha = 0
        connectImageView.alpha = 0
        connectButton.isEnabled = true
        connectImageView.layer.removeAllAnimations()
        roundLineView.layer.removeAllAnimations()

        UIView.animate(withDuration: 0.3, delay: 0, options: UIView.AnimationOptions.curveLinear, animations: {
            self.connectImageView.alpha = 1
            self.roundLineView.alpha = 1
        }) { (Bool) in }
        
        animateConnecting()
    }
    func stopConnectAnimation() {
        angle = 0
        connectImageView.alpha = 1
        connectButton.isEnabled = true
        connectImageView.layer.removeAllAnimations()
        roundLineView.layer.removeAllAnimations()

        UIView.animate(withDuration: 0.3, delay: 0, options: UIView.AnimationOptions.curveLinear, animations: {
            self.connectImageView.alpha = 0
            self.roundLineView.alpha = 0
        }, completion:  { (finished) -> Void in
            self.updateButtonFinalStatus()
        })
        
        //animateConnecting()
    }
    func animateConnecting() {
        let f : Double = Double(self.angle) * (Double.pi / 180.0)
        let  endAngle = CGAffineTransform(rotationAngle: CGFloat(f))
        UIView.animate(withDuration: 0.06, delay: 0, options: [.curveLinear, .allowUserInteraction], animations: {
            self.connectImageView.transform = endAngle;
        }, completion:  { (finished) -> Void in
        // ....
            if (finished) {
            self.angle -= 10
            self.animateConnecting()
            }
        })
    }
    func updateButtonFinalStatus() {
        if connectStatus == 2 {
            connectButton.setTitle("Disconnect", for: UIControl.State.normal)
            UIView.transition(with: connectButton, duration: 0.3, options: UIView.AnimationOptions.transitionCrossDissolve, animations: {
                //self.connectButton.setBackgroundImage(UIImage(named: "connectedButton"), for: UIControl.State.normal)
                //self.connectButton.setBackgroundImage(UIImage(named: "connectedButton"), for: UIControl.State.highlighted)
                self.connectButton.setImage(UIImage(named: "connectedButton"), for: UIControl.State.normal)
                self.connectButton.setTitleColor(UIColor.white, for: UIControl.State.normal)
            }, completion: nil)
            UIView.animate(withDuration: 0.3, delay: 0, options: UIView.AnimationOptions.curveLinear, animations: {
                self.roundConnectedView.alpha = 1
                self.roundLineView.alpha = 0
                self.connectImageView.alpha = 0
            }, completion: nil)
        } else if connectStatus == 0 {
            connectButton.setTitle("Connect", for: UIControl.State.normal)
            UIView.transition(with: connectButton, duration: 0.3, options: UIView.AnimationOptions.transitionCrossDissolve, animations: {
                //self.connectButton.setBackgroundImage(UIImage(named: "connButton"), for: UIControl.State.normal)
                //self.connectButton.setBackgroundImage(UIImage(named: "connButton"), for: UIControl.State.highlighted)
                self.connectButton.setImage(UIImage(named: "connButton"), for: UIControl.State.normal)
                //self.connectButton.setTitleColor(UIColor.white, for: UIControl.State.normal)
                self.connectButton.setTitleColor(UIColor.green, for: UIControl.State.normal)
            }, completion: nil)
            UIView.animate(withDuration: 0.3, delay: 0, options: UIView.AnimationOptions.curveLinear, animations: {
                self.roundConnectedView.alpha = 0
                self.roundLineView.alpha = 0
                self.connectImageView.alpha = 0
            }, completion: nil)
        }
        print("update button:", connectStatus)
    }
    
    @IBAction func login(_ sender: UIButton) {
        if g_premium != 0 {
            if (swiVpn.isOn == true) {
                return
            }
            self.txtUserName.isEnabled = true
            self.txtUserName.alpha = 1.0
            self.txtPassword.isEnabled = true
            self.txtPassword.alpha = 1.0
            //self.btnLogin.titleLabel?.text = "Login"
            self.btnLogin.setTitle("Login", for: .normal)
            self.txtUserStatus.text = "unregisted user."
            self.txtMonth.text = ""
            self.txtToday.text = ""
            self.g_premium = 0
            return
        }
        
        let service = NetworkService()
        let dispatchQueue = DispatchQueue(label: "QueueIdentification", qos: .background)
        let observer = UnsafeMutableRawPointer(Unmanaged.passUnretained(self).toOpaque())
        let username = self.txtUserName?.text
        let password = self.txtPassword?.text
        NSLog("tvpn start login, \(username as String?), \(password as String?)")
        dispatchQueue.async {
            service.login(username, pwd: password, device_id: self.hardwareUUID(),
                traffic_call: { (todayTraffic:Int,monthTraffic:Int,dayLimit:Int,monthLimit:Int,ret1:Int,ret2:Int,observer) in
                let mySelf = Unmanaged<ViewController>.fromOpaque(observer!).takeUnretainedValue()
                DispatchQueue.main.async{
                    if (ret1 == 0) {
                        if (ret2 == 1) {
                             mySelf.txtUserStatus.text = "basic user login ok."
                        } else if (ret2==2) {
                             mySelf.txtUserStatus.text = "premium user login ok."
                        }
                        mySelf.g_premium = ret2
                        mySelf.txtUserName.isEnabled = false
                        mySelf.txtPassword.isEnabled = false
                        mySelf.txtUserName.alpha = 0.5
                        mySelf.txtPassword.alpha = 0.5
                        
                        //mySelf.btnLogin.titleLabel?.minimumScaleFactor = 0.5
                        //mySelf.btnLogin.titleLabel?.numberOfLines = 0
                        //mySelf.btnLogin.titleLabel?.adjustsFontSizeToFitWidth = true
                        mySelf.btnLogin.setTitle("Logout", for: .normal) //titleLabel?.text = "Logout"

                        mySelf.g_day_limit = dayLimit
                        mySelf.g_month_limit = monthLimit
                         //btnSubLaunch.setEnabled(true)
                    } else {
                         mySelf.txtUserStatus.text = "login fail."
                         mySelf.txtUserName.text = ""
                         mySelf.txtPassword.text = ""
                    }
                    //print("show traffic")
                    mySelf.show_traffic(d: todayTraffic, m: monthTraffic)
                    let text = "login ok.\n"
                    mySelf.g_outputStream.write(text, maxLength: text.count)
                }
            }, withTarget: observer)

        }
        let defaults = UserDefaults.standard

        defaults.set(username!, forKey: "UserName")
        defaults.set(password!, forKey: "Password")
        print("save ok",username!,password!)
        return;
    }
    
    @IBAction func premium(_ sender: UIButton) {
        print(data.count)
        for section in data {
            //let section = data[indexPath.section]
                
                // Only available products can be bought.
            if section.type == .availableProducts {
                let content = section.elements// as? [SKProduct]
                for product in content {
                    let p = product as! SKProduct
                    //let product = content[indexPath.row]
                    // Attempt to purchase the tapped product.
                    print(p.localizedTitle,p.productIdentifier)
                    if p.productIdentifier == "renew_one_year" {
                        StoreObserver.shared.buy(p)
                    }
                }
            }
        }
    }
    

}

/// Extends ParentViewController to conform to StoreManagerDelegate.
extension ViewController: StoreManagerDelegate {
    func storeManagerDidReceiveResponse(_ response: [Section]) {
        //products.reload(with: response)
        print("recv response", response.count)
        data = response
    }
    
    func storeManagerDidReceiveMessage(_ message: String) {

    }
}

/// Extends ParentViewController to conform to StoreObserverDelegate.
extension ViewController: StoreObserverDelegate {
    func storeObserverDidReceiveMessage(_ message: String) {
        if message != "handlePurchased" {
            NSLog("message:\(message)")
            return
        }
        let service = NetworkService()
        let dispatchQueue = DispatchQueue(label: "QueueIdentification", qos: .background)
        let observer = UnsafeMutableRawPointer(Unmanaged.passUnretained(self).toOpaque())
        NSLog("tvpn purchase ok.")
        let username = self.txtUserName?.text
        dispatchQueue.async {
            service.premium(username,
                traffic_call: { (todayTraffic:Int,monthTraffic:Int,dayLimit:Int,monthLimit:Int,ret1:Int,ret2:Int,observer) in
                    let mySelf = Unmanaged<ViewController>.fromOpaque(observer!).takeUnretainedValue()
                    DispatchQueue.main.async{
                        if (ret1 == 0) {
                            if (ret2 == 1) {
                                mySelf.txtUserStatus.text = "basic user login ok."
                            } else if (ret2==2) {
                                mySelf.txtUserStatus.text = "premium user login ok."
                            }
                            mySelf.g_premium = ret2
                            mySelf.txtUserName.isEnabled = false
                            mySelf.txtPassword.isEnabled = false
                            mySelf.txtUserName.alpha = 0.5
                            mySelf.txtPassword.alpha = 0.5
                            
                            //mySelf.btnLogin.titleLabel?.minimumScaleFactor = 0.5
                            //mySelf.btnLogin.titleLabel?.numberOfLines = 0
                            //mySelf.btnLogin.titleLabel?.adjustsFontSizeToFitWidth = true
                            mySelf.btnLogin.setTitle("Logout", for: .normal) //titleLabel?.text = "Logout"
                            
                            mySelf.g_day_limit = dayLimit
                            mySelf.g_month_limit = monthLimit
                            //btnSubLaunch.setEnabled(true)
                        } else {
                            mySelf.txtUserStatus.text = "login fail."
                            mySelf.txtUserName.text = ""
                            mySelf.txtPassword.text = ""
                        }
                        //print("show traffic")
                        mySelf.show_traffic(d: todayTraffic, m: monthTraffic)
                        let text = "login ok.\n"
                        mySelf.g_outputStream.write(text, maxLength: text.count)
                    }
                }, withTarget: observer)
        }
    }
    
    func storeObserverRestoreDidSucceed() {

    }
}
