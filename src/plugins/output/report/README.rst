Report (output plugin)
=====================

The plugin provides a way to quickly gather information about connections with exporters.
It processes incoming Session and IPFIX Messages and records information about sessions, the
contexts used in the session, and the templates used in such contexts. 
It records information such as when the session opened and closed, which ODIDs were used in 
the session, which templates were used under the ODID, and how many times, when was the template 
first and last seen, when was it used and how many times, how many IPFIX Messages were lost, 
and similiar. It also checks for some common problems such as missing information element definitons
or misconfigured time on the exporter and warns about them. A summary of the information is then
generated in a form of a HTML page and saved to the filename provided in the configuration
upon exit.

Example configuration
---------------------

.. code-block:: xml
    <output>
        <name>Report output</name>
        <plugin>report</plugin>
        <params>
            <filename>/tmp/report.html</filename>
        </params>
    </output>

Parameters
----------

:``filename``:
    Output path of the generated report file.
