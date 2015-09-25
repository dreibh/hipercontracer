22 * * * *   root   if [ -e /etc/nornet/trace-configuration ] ; then /usr/bin/Random-Sleep 0 1800 -quiet && /usr/bin/NorNet-Trace-Import >>/var/log/nornet-trace-import.log 2>&1 ; fi
