# Overview output plugin


Provides an overview of elements that the collector is receiving.

The overview is printed to stdout upon exit.


Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>Overview output</name>
        <plugin>overview</plugin>
        <params>
            <skipOptionTemplates>false</skipOptionTemplates> <!-- default value -->
        </params>
    </output>

Parameters
----------

:``skipOptionsTemplates``:
    Options templates and options template records are skipped. [default: false]
