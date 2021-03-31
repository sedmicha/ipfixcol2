Aggregator (output plugin)
=====================

The plugin provides a means to aggregate IPFIX data.

Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>Aggregator output</name>
        <plugin>aggregator</plugin>
        <params>
            <activeTimer>60</activeTimer>
            <passiveTimer>600</passiveTimer>
            <view>
                <field>
                    <elem>sourceIPv4Address</elem>
                </field>
                <field>
                    <elem>destinationIPv4Address</elem>
                </field>
                <field>
                    <elem>sourceTransportPort</elem>
                </field>
                <field>
                    <elem>destinationTransportPort</elem>
                </field>
                <field>
                    <elem>octetDeltaCount</elem>
                    <aggregate>sum</aggregate>
                </field>
                <field>
                    <elem>packetDeltaCount</elem>
                    <aggregate>sum</aggregate>
                </field>
                <field>
                    <elem>deltaFlowCount</elem>
                    <aggregate>sum</aggregate>
                </field>
                <field>
                    <name>numDomainLookups</name>
                    <elem>cesnet:DNSName</elem>
                    <transform>firstLevelDomain</transform>
                    <aggregate>countUnique</aggregate>
                </field>
            </view>
            <view>
                <!-- ... -->
            </view>
        </params>
    </output>

Parameters
----------

:``activeTimer``:
    Interval of an active timer in seconds.

:``passiveTimer``:
    Interval of a passive timer in seconds.

