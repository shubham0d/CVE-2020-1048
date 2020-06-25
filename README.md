# CVE-2020-1048
POC exploit code for CVE-2020-1048(PrintDemon)

## Vulnerable Systems
All Windows version below mentioned in the link are vulnerable: https://portal.msrc.microsoft.com/en-US/security-guidance/advisory/CVE-2020-1048
## Steps to reproduce
1) Change the variables `g_PortName` and `g_InputFile` present at the top of `Source.c`.
2) Compile and run using Visual studio.
3) Restart the printer service(spoolsv) or Restart your system.

## Notes
* In case you want to persist the port and printer creation or want the attack to occur after restart of system, you can exit the program using Ctrl^C once the program ask to press Enter.
* `getshell.dll` is included which you can use as a payload to spawn a command prompt as `SYSTEM` privilege.
# Reference
https://windows-internals.com/printdemon-cve-2020-1048/
<br/>
https://github.com/ionescu007/PrintDemon
