#!/usr/bin/env pwsh

# Send an email using Powershell.
# Usage: sendmail.ps1 [attachment 1 [attachment 2 [... attachment N]]]
#
# This is a POC on how to send an email using Powershell. The script should work
# on all platforms. The information required for the script to work is all
# supplied via environment variables.
#
# Env Vars
#   smtp_host
#   smtp_port (best if you let the implementation decide)
#   smtp_tls:
#     'F' to insist on secure connection (default)
#     'O' for opportunistic
#     'N' to disable (default if $smtp_host is "localhost")
#   smtp_tls_cert
#   smtp_username
#   smtp_password
#   mail_from (required)
#   mail_to (required)
#   mail_subject (required)
#
# Example
#```pwsh
#   echo 'Hi! it going? Testing my Powershell script.' | \
#     smtp_host=smtp.gmail.com \
#     smtp_username=example@gmail.com \
#     smtp_password='0123456789' \
#     mail_from=alice@gmail.com \
#     mail_to=bob@example.com \
#     mail_subject='Sent using Powershell' \
#      sendmail.ps1 \
#        doc.pdf
#```
using namespace System
using namespace System.Net
using namespace System.Security.Cryptography

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$PSDefaultParameterValues['*:ErrorAction'] = 'Stop'

<#
.SYNOPSIS
Get an environment variable and unset it

.PARAMETER Name
The name of the environment variable to read and unset

.PARAMETER Required
If set, throw FileNotFoundException if the environment variable requested is not
set

.NOTES
The purpose of the function is to get and scrub off the password passed as an
env var in one go. To preserve the env var, use `GetEnvironmentVariable()`
directly.
#>
function FetchEnv ([string]$Name, [bool]$Required = $false) {
	$RetVal = [Environment]::GetEnvironmentVariable($Name)
	[Environment]::SetEnvironmentVariable($Name, '')
	if ( $RetVal ) {
		return $RetVal
	}
	else {
		if ($Required) {
			throw New-Object IO.FileNotFoundException ("${Name}: unset env var")
		}
		else {
			return $null
		}
	}
}

<#
.SYNOPSIS
Read data from STDIN until EOF and return data as decoded string
#>
function ExhaustStdin () {
	$stream = New-Object IO.StreamReader ( [Console]::OpenStandardInput() )
	return $stream.ReadToEnd()
}


######################################################################
# Execution starts here
######################################################################

# Compose a message
$mail = New-Object Mail.MailMessage (
	(FetchEnv "mail_from" $true),
	(FetchEnv "mail_to" $true),
	(FetchEnv "mail_subject" $true),
	(ExhaustStdin)) # This is the part where the mail body is read from STDIN

# Add attachments
foreach ($file in $args) {
	[string]$file = $file

	$a = New-Object Mail.Attachment (
		$file,
		# Treat all attachments as binary
		[System.Net.Mime.MediaTypeNames+Application]::Octet)
	# Timestamp support
	$a.ContentDisposition.CreationDate = [IO.File]::GetCreationTime($file)
	$a.ContentDisposition.ModificationDate = [IO.File]::GetLastWriteTime($file)
	$a.ContentDisposition.ReadDate = [IO.File]::GetLastAccessTime($file)

	$mail.Attachments.Add($a)
}

# Set up credentials
$client_cred = New-Object NetworkCredential (
	(FetchEnv "smtp_username"),
	(FetchEnv "smtp_password"))

# Set up client TLS cert
$tls_cert = FetchEnv("smtp_tls_cert")
if ($null -ne $tls_cert) {
	$cert_chain = New-Object X509Certificates.X509Certificate ( $tls_cert )
}
else {
	$cert_chain = $null
}

# Read target SMTP host
$smtp_host = FetchEnv "smtp_host"
if (!$smtp_host) {
	$smtp_host = "localhost"
}

# Set up SMTP client object
$smtp = New-Object Mail.SmtpClient ($smtp_host)
if ($cert_chain) {
	$smtp.ClientCertificates.Add($cert_chain)
}
$smtp.Credentials = $client_cred
$smtp_port = FetchEnv "smtp_port"
if ($smtp_port) {
	$smtp.Port = [int]$smtp_port
}

$tlsmode = FetchEnv "smtp_tls"
if (!$tlsmode) {
	# Determine the "tlsmode" to default to
	if ($smtp_host -eq "localhost") {
		# No need to waste computing power on TLS.
		# Unless you're paranoid and don't trust the hosts file.
		$tlsmode = "N"
	}
	else {
		# Transmitting plain text data on the internet nowadays is no-brainer.
		# Most email services will refuse anyway.
		$tlsmode = "F"
	}
}

# Set `$smtp.EnableSsl` based on `$tlsmode`
if ($tlsmode -eq "F" -or $tlsmode -eq "O") {
	$smtp.EnableSsl = $true
}
elseif ($tlsmode -eq "N") {
	$smtp.EnableSsl = $false
}

try {
	$smtp.Send($mail)
}
catch {
	if ($tlsmode -eq "O") {
		# Opportunistic tlsmode. Try again with TLS disabled.
		# Please think twice and fix the problem before resorting to this bit.
		$smtp.EnableSsl = $false
		$smtp.Send($mail)
	}
	else {
		# Let the script die
		throw $_
	}
}
