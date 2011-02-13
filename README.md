OpenPS3FTP
==========
*An open source FTP server for the PlayStation 3*  
*first homebrew by [@jjolano](http://twitter.com/jjolano)*

Licensed under the GNU General Public License version 3.  
Source code available at [github](https://github.com/jjolano/openps3ftp).

OpenPS3FTP was built using the PSL1GHT homebrew SDK.

**Default login details:**  
`Username: root` `Password: openbox`

Credit to PSL1GHT, geohot, and stoneMcClane for tools and code.


Instructions
-----------------
Welcome to the `README` - an important file that you really _should_ read.  
Since some users don't know how to install or use OpenPS3FTP, I will provide instructions here.  

**Installation**  
1. Extract the `pkg` file from the archive. `openps3ftp.pkg` should work just fine for those on 3.55 kmeaw. Else, try `openps3ftp.geohot.pkg`.  
2. Copy the `pkg` file onto a USB drive and then plug that USB drive into your console.  
3. Select the `Install Package Files` item under the Game menu of the XMB.  
4. Select `openps3ftp.pkg` and answer 'yes' if asked to overwrite.  
5. OpenPS3FTP is now installed!  

**Connection**  
1. Now that OpenPS3FTP is installed, select the `OpenPS3FTP` item under the Game menu of the XMB.  
2. On your computer, open an FTP client program like [FileZilla](http://filezilla-project.org/) and connect to your console using the console's IP address and the login details provided. You can find the IP address by going to Network Settings and selecting the connection status option.  
3. Once connected, you should see a list of folders in your FTP client. You can browse through any of these folders and upload or download files - basically remote file management. (hint: your hard drive is at `/dev_hdd0/`)  
4. That's it! That's the basic usage of FTP. If you want to, you can change the default password by entering a custom command: `PASSWD <newpassword>`  

**Compilation**  
To compile OpenPS3FTP, all you will need to install is the latest version of [ps3toolchain](https://github.com/ps3dev/ps3toolchain), which will also include the PSL1GHT SDK and ps3libraries.


Contributions
---------------
You are encouraged to fork this repository, add improvements to the code, and submit a pull request. With your help, OpenPS3FTP may become the best open source FTP server for the PlayStation 3.

I can accept personal donations as a thank-you for this homebrew. [Donate](http://bit.ly/gmzGcI)

---------------------------------------
Thank you for your support and use of OpenPS3FTP.

