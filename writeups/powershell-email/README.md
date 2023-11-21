# Sending Mail using Powershell
Turns out, Powershell can be used to send emails through harnessing the power of
C#. I made this script as a POC as to show how far .Net and Powershell have
come.

Should work on all platforms that support Powershell.

## Usage
Copied from the script.
```sh
echo 'Hi! it going? Testing my Powershell script.' | \
  smtp_host=smtp.gmail.com \
  smtp_username=example@gmail.com \
  smtp_password='0123456789' \
  mail_from=alice@gmail.com \
  mail_to=bob@example.com \
  mail_subject='Sent using Powershell' \
   sendmail.ps1 \
     doc.pdf
```

## Few Tips
### Password
Services like Gmail will require you to get a separate password for external
apps. Google calls this "App password". Refer to the links below.

* https://support.google.com/accounts/answer/185833
* https://support.google.com/mail/answer/7126229

Even if the normal password for the account can be used, a separate password
should always be used for program access. Always check if your email provider
supports this.

### TLS
Most services will refuse to serve on unsecure connections. Use `smtp_tls=O`
only as the last resort.

`smtp_tls_cert` is for TLS CN SASL authentication. if authenticating using this
method, `smtp_username` and `smtp_password` are not required.

### CC and More
Didn't think about CC and all the advanced composition. Feel free to add more
feature to the script that is already monstrous!
