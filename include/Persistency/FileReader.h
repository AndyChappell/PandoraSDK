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
     */
    StatusCode ReadGlobalHeader();

    StatusCode ReadGeometry();
    StatusCode ReadEvent();
    StatusCode GoToGlobalHeader();
    StatusCode GoToNextGeometry();
    StatusCode GoToNextEvent();

    virtual StatusCode GoToGeometry(const unsigned int geometryNumber) = 0;
    virtual StatusCode GoToEvent(const unsigned int eventNumber) = 0;

protected:
    virtual StatusCode ReadHeader() = 0;
    virtual StatusCode GoToNextContainer() = 0;
    virtual ContainerId GetNextContainerId() = 0;
    virtual StatusCode ReadNextGlobalHeaderComponent() = 0;
    virtual StatusCode ReadNextGeometryComponent() = 0;
    virtual StatusCode ReadNextEventComponent() = 0;
};

} // namespace pandora

#endif // #ifndef PANDORA_FILE_READER_H
