Viewer (output plugin)
=====================

The plugin converts IPFIX Messages into plain text and prints them on standard output.

The main goal of the plugin is to show content of received IPFIX Messages in human readable form.
Each IPFIX Message is broken down into IPFIX Sets and each IPFIX Set is further broken down into
(Options) Template/Data records and so on. Fields of the Data records are formatted according
to the expected data type, if their corresponding Information Element definitions are known to
the collector. Therefore, the output can be also used to determine missing Information Element
definitions.

Biflow records and structured data types are also supported and appropriately formatted.
Output is not supposed to be further parsed because its format can be changed in the future.
However, if you are interested into processing Data Records, consider using other
plugins such as JSON, UniRec, etc.

Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>Viewer output</name>
        <plugin>viewer</plugin>
        <params/>
    </output>

Parameters
----------

Parameters are not supported by the plugin.
