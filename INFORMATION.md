PSU - Program Specification
Overview
PSU (Pseudo Scripting Utility) is a lightweight interpreted scripting language with a simple syntax for system automation, file manipulation, networking, and HTTP operations. It features a two-pass execution model (validation then execution) similar to PHP.

Language Syntax
Comments
psu
// This is a comment
var x = 10  // Inline comment
Program Structure
Every PSU program must end with the psstart token:

psu
// Code goes here
psstart
Basic Commands
1. Library Loading (psload)
psu
psload <library_name>
Available Libraries:

Library	Description
system	System information functions
math	Mathematical operations and constants
filesys	File and directory operations
net	Network functions
http	HTTP client and server
Example:

psu
psload system
psload math
psload http
2. Variable Assignment (var)
psu
var <name> = <value>
Types:

Strings: var name = 'John'

System calls: var time = system.time

Math expressions: var result = 10 * 5

HTTP calls: var ip = http.get("https://api.ipify.org")

Network calls: var dns = net.dns("google.com")

Other variables: var y = x

Example:

psu
var greeting = 'Hello World'
var time = system.time
var pi = math.pi
var ip = http.get("https://api.ipify.org")
3. Output (echo)
psu
echo <expression>
Single Argument:

psu
echo 'Hello World'        // String literal
echo system.time          // System function
echo ip                   // Variable
echo 10 * 5               // Math expression
Concatenation with ++:

psu
echo 'My IP: ' ++ ip
echo 'Time: ' ++ system.time
echo 'Result: ' ++ (10 * 5)
Multiple Arguments (without ++):

psu
echo 'Hello' 'World'      // Prints: HelloWorld
4. Functions (def/enddef)
Definition:

psu
def function_name
    // function body
    // can contain any valid PSU commands
enddef
Calling:

psu
function_name
Rules:

Functions must be defined before they're called (top-to-bottom execution)

No parameters are supported (uses global variables)

Function body is only executed when called

Example:

psu
def greet_user
    var name = system.username
    echo 'Hello ' ++ name
enddef

greet_user
Libraries Reference
1. System Library (psload system)
Function	Description	Example Output
system.time	Current time	14:30:25
system.date	Current date	2026-09-08
system.os	OS, kernel, architecture	Linux 6.12.93 (aarch64)
system.hostname	System hostname	my-pc
system.username	Current username	mikx
system.ip4	IPv4 address	192.168.1.68
system.ip6	IPv6 address	fe80::7649:756a:3672
Example:

psu
psload system
echo 'Host: ' ++ system.hostname
echo 'User: ' ++ system.username
echo 'Time: ' ++ system.time
2. Math Library (psload math)
Constants:

Constant	Value
math.pi	3.141592653589793
math.e	2.718281828459045
math.sqrt2	1.414213562373095
math.ln2	0.693147180559945
Operators:

Addition: +

Subtraction: -

Multiplication: *

Division: /

Parentheses: ( )

Example:

psu
psload math
var area = math.pi * 5 * 5
echo 'Area: ' ++ area
echo 10 + 5 * 2
var result = (5 + 3) * 4
3. Filesystem Library (psload filesys)
Function	Description	Example
filesys.pwd	Current directory	echo filesys.pwd → /home/user
filesys.dir	List directory contents	echo filesys.dir → file1.txt dir1/
filesys.touch(filename)	Create empty file	filesys.touch("test.txt")
filesys.write(filename) "content"	Write to file	filesys.write("file.txt") "Hello"
filesys.read(filename)	Read file content	echo filesys.read("file.txt") → Hello
filesys.perm(filename)	File permissions	echo filesys.perm("file.txt") → -rw-r--r--
Examples:

psu
psload filesys
echo 'Current: ' ++ filesys.pwd
echo 'Files: ' ++ filesys.dir
filesys.touch("test.txt")
filesys.write("test.txt") "Hello World"
echo filesys.read("test.txt")
echo filesys.perm("test.txt")
4. Network Library (psload net)
Function	Description	Example
net.ip4	IPv4 address	echo net.ip4 → 192.168.1.68
net.ip6	IPv6 address	echo net.ip6 → fe80::7649:756a:3672
net.ifaces	List network interfaces	echo net.ifaces → eth0: 192.168.1.68
net.ping(host)	Ping a host	net.ping("google.com")
net.dns(host)	DNS lookup	echo net.dns("google.com") → 142.250.185.46
net.mac(interface)	MAC address	echo net.mac("eth0") → 00:11:22:33:44:55
Examples:

psu
psload net
echo 'My IP: ' ++ net.ip4
echo 'Interfaces: ' ++ net.ifaces
echo 'DNS: ' ++ net.dns("google.com")
net.ping("google.com")
5. HTTP Library (psload http)
HTTP Client Functions
Function	Description	Example
http.get(url)	HTTP GET request	echo http.get("https://api.ipify.org")
http.head(url)	HTTP HEAD request	echo http.head("https://example.com")
Examples:

psu
psload http
var ip = http.get("https://api.ipify.org")
echo 'Public IP: ' ++ ip
var headers = http.head("https://example.com")
echo 'Headers: ' ++ headers
HTTP Server
Start Server:

psu
http.server(port)
Define Routes:

psu
http.method("/path") "response"
Methods: get, head, post, put, delete

Example Server:

psu
psload http

// Define routes
http.get('/') "<h1>Welcome to PSU Server!</h1>"
http.get('/hello') "<h1>Hello Bestie!</h1>"
http.get('/api') "{\"status\":\"ok\",\"message\":\"Hello from PSU\"}"

// Start server
http.server(8080)
psstart
Execution Model
Two-Pass System
Validation Pass:

All syntax errors are detected

Functions are registered

Libraries are loaded

If ANY error is found, execution stops

Execution Pass:

Only runs if validation passed with NO errors

Top-to-bottom execution

Functions must be defined before they're called

Error Handling
PSU uses PHP/Java-like error messages:

text
Compilation Error in test.psu on line 3: Unknown library 'nonexistent'
Runtime Error in test.psu on line 5: Variable 'x' is undefined.
Syntax Error in test.psu on line 7: Missing 'enddef' for function 'test'
Parse Error in test.psu on line 9: Expected '=' after variable name.
Complete Examples
Example 1: System Information
psu
psload system

echo '=== System Information ==='
echo 'Hostname: ' ++ system.hostname
echo 'Username: ' ++ system.username
echo 'OS: ' ++ system.os
echo 'Date: ' ++ system.date
echo 'Time: ' ++ system.time
echo 'IPv4: ' ++ system.ip4
echo 'IPv6: ' ++ system.ip6
psstart
Example 2: File Operations
psu
psload filesys
psload system

def create_report
    var name = system.username
    var date = system.date
    var filename = 'report_' ++ name ++ '.txt'
    filesys.write(filename) "Report for " ++ name ++ " on " ++ date
    echo 'Created: ' ++ filename
enddef

create_report
echo 'Current files: ' ++ filesys.dir
psstart
Example 3: Web Request
psu
psload http
psload net

var ip = http.get("https://api.ipify.org")
echo 'My public IP: ' ++ ip

var headers = http.head("https://example.com")
echo 'Headers:' ++ headers
psstart
Example 4: HTTP Server
psu
psload http
psload system

http.get('/') "<h1>Hello from " ++ system.hostname ++ "!</h1>"
http.get('/time') "Current time: " ++ system.time
http.get('/api') "{\"status\":\"ok\",\"server\":\"" ++ system.hostname ++ "\"}"

echo 'Starting HTTP Server on port 8080...'
http.server(8080)
psstart
Example 5: Combined Usage
psu
psload system
psload filesys
psload net
psload http

def check_system
    var host = system.hostname
    var user = system.username
    var date = system.date
    var ip = net.ip4
    
    filesys.write("status.txt") "Host: " ++ host ++ "\nUser: " ++ user ++ "\nDate: " ++ date ++ "\nIP: " ++ ip
    echo 'Status file created'
enddef

def get_public_ip
    var public_ip = http.get("https://api.ipify.org")
    echo 'Public IP: ' ++ public_ip
    return public_ip
enddef

check_system
var pub = get_public_ip
echo 'Done!'
psstart
Rules Summary
Top-to-bottom execution - Functions must be defined before being called

Two-pass validation - Syntax checked first, then executed

No JIT execution - If any error found, program doesn't run

Libraries must be loaded before using their functions

String literals require single quotes: 'text'

URLs in HTTP requests must be in quotes

Concatenation uses ++ operator

Comments use // (single line only)

Every program must end with psstart

Variable names are case-sensitive

No variable type declarations - dynamically typed

Error Messages
Error Type	Example	Cause
Compilation Error	Unknown library 'xyz'	Library not recognized
Runtime Error	Variable 'x' is undefined	Variable used before assignment
Syntax Error	Missing 'enddef'	Function definition incomplete
Parse Error	Expected '=' after variable name	Invalid variable assignment
Math Error	Invalid expression	Math syntax error