/**
 *  @file   PandoraSDK/include/Persistency/FileReader.h
 *
 *  @brief  Header file for the file reader class.
 *
 *  $Log: $
 */
#ifndef PANDORA_FILE_READER_H
#define PANDORA_FILE_READER_H 1

#include "Pandora/StatusCodes.h"

#include "Persistency/PandoraIO.h"
#include "Persistency/Persistency.h"

#include <string>

namespace pandora
{

class Pandora;

//------------------------------------------------------------------------------------------------------------------------------------------

/**
 *  @brief  FileReader class
 */
class FileReader : public Persistency
{
public:
    /**
     *  @brief  Constructor
     *
     *  @param  pandora   the pandora instance to be used alongside the file reader
     *  @param  fileName  the name of the file containing the pandora objects
     */
    FileReader(const pandora::Pandora &pandora, const std::string &fileName);

    /**
     *  @brief  Destructor
     */
    virtual ~FileReader();

    /**
     *  @brief  Read the global header from the file, populating m_metadata and
     *          m_schemaRegistry on the base Persistency object.
     *
     *          Should be called once before ReadGeometry() or ReadEvent() so
     *          that per-component schema versions are known before any component
     *          data is read. Files written without a global header (or with only
     *          a legacy VERSION_COMPONENT) are handled gracefully: missing
     *          metadata fields default to empty strings and the schema registry
     *          remains empty, causing all readers to use their built-in current
     *          schema versions with no migrations applied.
     *
     *          EventReadingAlgorithm calls this in Initialize().
     */
    StatusCode ReadGlobalHeader();

    /**
     *  @brief  Read the current geometry information from the file
     */
    StatusCode ReadGeometry();

    /**
     *  @brief  Read an entire pandora event from the file, recreating the stored objects
     */
    StatusCode ReadEvent();

    /**
     *  @brief  Skip to global header container in the file
     */
    StatusCode GoToGlobalHeader();

    /**
     *  @brief  Skip to next geometry container in the file
     */
    StatusCode GoToNextGeometry();

    /**
     *  @brief  Skip to next event container in the file
     */
    StatusCode GoToNextEvent();

    /**
     *  @brief  Skip to a specified geometry number in the file
     *
     *  @param  geometryNumber the geometry number
     */
    virtual StatusCode GoToGeometry(const unsigned int geometryNumber) = 0;

    /**
     *  @brief  Skip to a specified event number in the file
     *
     *  @param  eventNumber the event number
     */
    virtual StatusCode GoToEvent(const unsigned int eventNumber) = 0;

protected:
    virtual StatusCode ReadHeader() = 0;
    virtual StatusCode GoToNextContainer() = 0;
    virtual ContainerId GetNextContainerId() = 0;
    virtual StatusCode ReadNextGlobalHeaderComponent() = 0;
    virtual StatusCode ReadNextGeometryComponent() = 0;
    virtual StatusCode ReadNextEventComponent() = 0;

    // CHANGE: m_fileMajorVersion and m_fileMinorVersion are retained for
    // backward compatibility with the XmlFileReader (which still reads them
    // from the legacy Version element) and any user code that inspects them.
    // In the new binary format the equivalent information is carried per-
    // component in the SchemaRegistry (on the Persistency base class), so
    // these members should be considered deprecated and will be removed once
    // the XML reader is updated.
    unsigned int m_fileMajorVersion; ///< Legacy file major version (deprecated; use SchemaRegistry)
    unsigned int m_fileMinorVersion; ///< Legacy file minor version (deprecated; use SchemaRegistry)
};

} // namespace pandora

#endif // #ifndef PANDORA_FILE_READER_H
