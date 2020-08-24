Profiles (intermediate plugin)
==============================

The plugin performs profiling and adds profiling information to the flow records.
This information should be later processed by output plugins to categorize the flow records into their corresponding profiles and channels for storage. 

Profiles file format
--------------------

The profiles file contains the definitions of profiles and channels used for the profiling process.
The format of the file is following:

.. code-block:: xml

    <profileTree>
        <profile name="live">
            <type>normal</type>
            <directory>/tmp/ipfixcol</directory>
            <channelList>
                <channel name="ch1">
                    <sourceList>
                        <source>*</source>
                    </sourceList>
                    <filter>ipv4</filter>
                </channel>
                <!-- ... more channels ... -->
            </channelList>
            <subprofileList>
                <profile name="web">
                    <type>normal</type>
                    <channelList>
                        <channel name="http">
                            <sourceList>
                                <source>ch1</source>
                            </sourceList>
                            <filter>port in [80, 8080]</filter>
                        </channel>
                    </channelList>
                </profile>
                <!-- ... more profiles ... -->
            </subprofileList>
        </profile>
    </profileTree>


The top profile is the `live` profile, which must always exist. Profiles contain one or more channels and zero or more subprofiles.
Each subprofile is also a profile, so the same applies. If a profile doesn't have `directory` set, the directory is deduced from the parent directory and the profile name. 
The root `live` profile must have a directory set.

Channels have a list of sources and a filter expression. Sources refer to the channels of the parent profile. If a flow record matches the channel the source is referring to, 
then the flow record is also propagated to this channel and tested against the filter. A special kind of source is `*`, which refers to all the channels of the parent profile.

For documentation of the filter expression, see the filter plugin.

Example configuration
---------------------

.. code-block:: xml

    <intermediate>
        <name>Profiles</name>
        <plugin>profiles</plugin>
        <params>
            <filename>profiles.xml</filename>
        </params>
    </intermediate>

Parameters
----------

:``filename``:
    Location of the file containing the profile definitions.
