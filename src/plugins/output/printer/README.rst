Printer (output plugin)
=====================

This plugin prints Data Records in IPFIX Messages to the standard output in a format specified by 
a formatting string. 

Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>Printer output</name>
        <plugin>printer</plugin>
        <params>
            <format>%{srcip,w=15}:%{srcport,w=-7} -> %{dstip,w=15}:%{dstport,w=-7}  %tcpflags  %octetDeltaCount  %duration</format> 
            <useLocalTime>true</useLocalTime>
            <splitBiflow>true</splitBiflow>
            <markBiflow>true</markBiflow>
        </params>
    </output>

Parameters
----------

:``format``:
    The formatting string which specifies how the Data Record output should be formatted.
    The formatting string behaves similarly to printf. Fields are specified by a % followed by 
    a field name, which can be the full IPFIX entity name (e.g. iana:octetCount, or just octetCount),
    an alias (as specified in aliases.xml, e.g. ip, port, flowstart, ...) or one of the predefined 
    special fields (odid, bps, pps, bpp, duration). A field may also specify a minimal width and 
    alignment of the field value. For better understanding, see the example configuration above.
    [required]

:``scaleNumbers``:
    Write out large numbers in their scaled form (k, M, G, T) where it makes sense. [default=true]

:``shortenIPv6Addresses``:
    Only write out some of the leading and trailing octets when writing IPv6 addresses. [default=true]

:``useLocalTime``:
    Convert the timestamps to local timezone when writing them out. [default=true]

:``splitBiflow``:
    Split data records containing biflow into two separate lines. [default=true]

:``markBiflow``:
    When splitting biflow records also indicate the direction of each of the lines. [default=true]

:``escapeMode``:
    Escape some special characters when writing out string values, for example when we want to
    format the data in a csv format. Possible values: "normal", "csv". [default=normal]

:``translateProtocols``:
    Write out protocol names instead of their number. [default=true]

:``translatePorts``:
    Write out service names instead of port numbers. [default=true]

:``translateTCPFlags``:
    Write out TCP flags in a human readable form instead of a raw value. [default=true] 

:``translateAddresses``:
    Perform reverse DNS lookup for IP addresses and write out their domain name instead.
    WARNING: This will probably be very slow. [default=false]

