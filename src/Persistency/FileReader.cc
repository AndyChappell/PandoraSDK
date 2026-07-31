/**
 *  @file   PandoraSDK/src/Persistency/FileReader.cc
 *
 *  @brief  Implementation of the file reader class.
 *
 *  $Log: $
 */

#include "Api/PandoraApi.h"

#include "Persistency/FileReader.h"

#include <iostream>

namespace pandora
{

FileReader::FileReader(const pandora::Pandora &pandora, const std::string &fileName) :
    Persistency(pandora, fileName),
    m_fileMajorVersion(1),
    m_fileMinorVersion(0)
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

FileReader::~FileReader()
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

// CHANGE: ReadGlobalHeader previously just ran the component loop and discarded
// whatever the subclass deserialisers stored. That worked when the global header
// only held a VERSION_COMPONENT (two ints), because those were written directly
// into m_fileMajorVersion / m_fileMinorVersion by BinaryFileReader::ReadVersion.
//
// With the new format the global header also contains a METADATA component and
// a SCHEMA_REGISTRY component, both of which write into m_metadata and
// m_schemaRegistry on the Persistency base. Those members are already there;
// ReadGlobalHeader just needs to ensure they are populated before returning.
//
// The component loop itself is unchanged — BinaryFileReader::ReadNextGlobalHeader
// Component dispatches to ReadMetadata / ReadSchemaRegistry / ReadVersion as
// appropriate, and each of those writes into the base-class members directly.
// So the only substantive change here is:
//
//   1. The file may not have a HEADER_CONTAINER at all (geometry-only files,
//      or files written by old code). We now tolerate that gracefully rather
//      than returning STATUS_CODE_FAILURE: if no header container is found we
//      log a notice and return success, leaving metadata/registry at defaults.
//
//   2. m_containerId is reset to UNKNOWN_CONTAINER at the end, as before.
//
// Everything else — the GoToGlobalHeader seek, the ReadHeader call, the loop —
// is structurally identical to the original.
StatusCode FileReader::ReadGlobalHeader()
{
    // If no header container is present (geometry/event-only file written by
    // old code), seek fails gracefully: we log and return success so the
    // caller can proceed to ReadGeometry / ReadEvent with default metadata.
    if (HEADER_CONTAINER != this->GetNextContainerId())
    {
        const StatusCode seekSc = this->GoToGlobalHeader();

        if (STATUS_CODE_SUCCESS != seekSc)
        {
            std::cout << "FileReader::ReadGlobalHeader() — no header container found; "
                      << "proceeding with default metadata and schema registry." << std::endl;
            return STATUS_CODE_SUCCESS;
        }
    }

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->ReadHeader());

    if (HEADER_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    try
    {
        while (STATUS_CODE_SUCCESS == this->ReadNextGlobalHeaderComponent())
            continue;
    }
    catch (StatusCodeException &statusCodeException)
    {
        std::cout << "FileReader::ReadGlobalHeader() encountered unrecognized component: "
                  << statusCodeException.ToString() << std::endl;
    }

    m_containerId = UNKNOWN_CONTAINER;

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// All methods below are unchanged from the original.
//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileReader::ReadGeometry()
{
    if (GEOMETRY_CONTAINER != this->GetNextContainerId())
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->GoToNextGeometry());
    }

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->ReadHeader());

    if (GEOMETRY_CONTAINER != m_containerId)
        return STATUS_CODE_FAILURE;

    try
    {
        while (STATUS_CODE_SUCCESS == this->ReadNextGeometryComponent())
            continue;
    }
    catch (StatusCodeException &statusCodeException)
    {
        std::cout << "FileReader::ReadGeometry() encountered unrecognized object in file: "
                  << statusCodeException.ToString() << std::endl;
    }

    m_containerId = UNKNOWN_CONTAINER;

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileReader::ReadEvent()
{
    if (EVENT_CONTAINER != this->GetNextContainerId())
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->GoToNextEvent());
    }

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->ReadHeader());

    try
    {
        while (STATUS_CODE_SUCCESS == this->ReadNextEventComponent())
            continue;
    }
    catch (StatusCodeException &statusCodeException)
    {
        std::cout << "FileReader::ReadEvent() encountered unrecognized object in file: "
                  << statusCodeException.ToString() << std::endl;
    }

    m_containerId = UNKNOWN_CONTAINER;

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileReader::GoToGlobalHeader()
{
    do
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->GoToNextContainer());
    } while (HEADER_CONTAINER != this->GetNextContainerId());

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileReader::GoToNextGeometry()
{
    do
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->GoToNextContainer());
    } while (GEOMETRY_CONTAINER != this->GetNextContainerId());

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileReader::GoToNextEvent()
{
    do
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->GoToNextContainer());
    } while (EVENT_CONTAINER != this->GetNextContainerId());

    return STATUS_CODE_SUCCESS;
}

} // namespace pandora
