OpenPS3FTP
==========
*An open source ftp server for the PlayStation 3*  
*first homebrew by [@jjolano](http://twitter.com/jjolano)*

Licensed under the GNU General Public License version 3.  
Source code available at: https://github.com/jjolano/openps3ftp

OpenPS3FTP was built using the PSL1GHT homebrew SDK.

**Default login details:**  
`Username: root` `Password: openbox`

Credit to PSL1GHT, geohot, and stoneMcClane for tools and code.


Instructions
-----------------
Welcome to the `README` - an important file that you really _should_ read.  
Since some users don't know how to install or use OpenPS3FTP, I will provide instructions here.

**Installation**  
1. Extract the `pkg` file from the archive. In most cases the file `openps3ftp.pkg` should work just fine.  
2. Copy the `pkg` file onto a USB drive and then plug that USB drive into your console.  
3. Select the `Install Package Files` item under the Game menu of the XMB.  
4. Select `openps3ftp.pkg` and answer 'yes' if asked to overwrite.  
5. OpenPS3FTP is now installed!  

**Connection**  
1. Now that OpenPS3FTP is installed, select the `OpenPS3FTP` item under the Game menu of the XMB.  
2. On your computer, open an FTP client program like [FileZilla](http://filezilla-project.org/) and connect to your console using the console's IP address and the login details provided. You can find the IP address by going to Network Settings and selecting the connection status option.  
3. Once connected, you should see a list of folders in your FTP client. You can browse through any of these folders and upload or download files - basically remote file management.  
4. That's it! That's the basic usage of FTP. If you want to, you can change the default password by entering a custom command: `PASSWD <newpassword>`  


Prerequisites
-------------------
To **compile** OpenPS3FTP, you will need a few things first:

* [ps3toolchain](https://github.com/ooPo/ps3toolchain)
* PSL1GHT SDK (included with ps3toolchain)


Contribute
---------------
Code contributions towards this homebrew are welcome. Just submit a pull request on github and I'll check it out ASAP.

As for contributions in terms of donations, if you are feeling generous and have some spare change, you can [buy me a beer](http://bit.ly/gmzGcI) =).

---------------------------------------
Thank you for your support and use of OpenPS3FTP.
This `README` is using the Markdown markup language, which is really cool in my opinion and looks great when displayed in github.
