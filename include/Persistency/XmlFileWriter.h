/**
 *  @file   PandoraSDK/include/Persistency/XmlFileWriter.h
 *
 *  @brief  Header file for the xml file writer class.
 *
 *  $Log: $
 */
#ifndef PANDORA_XML_FILE_WRITER_H
#define PANDORA_XML_FILE_WRITER_H 1

#include "Pandora/Pandora.h"

#include "Objects/CartesianVector.h"
#include "Objects/TrackState.h"

#include "Persistency/FileWriter.h"
#include "Persistency/Persistency.h"   // FieldMap

#include "Xml/tinyxml.h"

namespace pandora
{

/**
 *  @brief  XmlFileWriter
 *
 *  Writes Pandora objects to a self-describing XML file. The document structure
 *  mirrors the binary format conceptually: each component is a named element
 *  carrying a schemaVersion attribute, whose child elements are the tagged fields.
 *
 *  Document structure
 *  ------------------
 *  <PandoraFile>
 *    <Header>
 *      <Version MajorVersion="1" MinorVersion="0"/>
 *      <Metadata schemaVersion="1">
 *        <producerName>...</producerName>
 *        ...
 *      </Metadata>
 *      <SchemaRegistry schemaVersion="0">
 *        <0>1</0>   <!-- ComponentId 0 (CaloHit) at schemaVersion 1 -->
 *        ...
 *      </SchemaRegistry>
 *    </Header>
 *    <Geometry>
 *      <LArTPC schemaVersion="1">
 *        <larTPCVolumeId>0</larTPCVolumeId>
 *        ...
 *      </LArTPC>
 *    </Geometry>
 *    <Event>
 *      <EventInfo schemaVersion="1"><run>0</run>...</EventInfo>
 *      <CaloHit schemaVersion="1"><positionVector>...</positionVector>...</CaloHit>
 *    </Event>
 *  </PandoraFile>
 *
 *  WriteVariable
 *  -------------
 *  The typed WriteVariable template is kept public for backward compatibility
 *  with legacy ObjectFactory::Write(object, FileWriter&) implementations.
 *  New factory code should override Write(object, FieldMap&) instead.
 */
class XmlFileWriter : public FileWriter
{
public:
    /**
     *  @brief  Constructor
     *
     *  @param  pandora      the pandora instance
     *  @param  fileName     the name of the output file
     *  @param  fileMode     APPEND (default) or OVERWRITE
     *  @param  majorVersion legacy major version written to the Version element
     *  @param  minorVersion legacy minor version written to the Version element
     */
    XmlFileWriter(const pandora::Pandora &pandora, const std::string &fileName,
        const FileMode fileMode = APPEND,
        const unsigned int majorVersion = 1, const unsigned int minorVersion = 0);

    /**
     *  @brief  Destructor — saves the XML document to disk
     */
    ~XmlFileWriter();

    /**
     *  @brief  Override WriteGlobalHeader to emit FileMetadata and SchemaRegistry
     *          in addition to the legacy Version element.
     */
    StatusCode WriteGlobalHeader();

    /**
     *  @brief  Write a typed value as a child element of m_pCurrentXmlElement.
     *          Retained for backward compatibility with legacy factory Write methods.
     */
    template <typename T>
    StatusCode WriteVariable(const std::string &xmlKey, const T &t);

private:
    StatusCode WriteHeader(const ContainerId containerId);
    StatusCode WriteFooter();
    StatusCode WriteVersion();
    StatusCode WriteMetadata();
    StatusCode WriteSchemaRegistry();

    /**
     *  @brief  Serialise a FieldMap as a named XML element with a schemaVersion
     *          attribute, appending it to m_pContainerXmlElement.
     */
    StatusCode WriteComponent(const std::string &elementName,
        const unsigned int schemaVersion, const FieldMap &fields);

    StatusCode WriteSubDetector(const SubDetector *const pSubDetector);
    StatusCode WriteLArTPC(const LArTPC *const pLArTPC);
    StatusCode WriteDetectorGap(const DetectorGap *const pDetectorGap);
    StatusCode WriteCaloHit(const CaloHit *const pCaloHit);
    StatusCode WriteTrack(const Track *const pTrack);
    StatusCode WriteMCParticle(const MCParticle *const pMCParticle);
    StatusCode WriteRelationship(const RelationshipId relationshipId,
        const void *address1, const void *address2, const float weight);
    StatusCode WriteEventInformation();

    static unsigned int GetSchemaVersion(const ComponentId componentId);

    TiXmlDocument *m_pXmlDocument;         ///< The XML document (owned)
    TiXmlElement  *m_pContainerXmlElement; ///< Current container element
    TiXmlElement  *m_pCurrentXmlElement;   ///< Current component element
};

//------------------------------------------------------------------------------------------------------------------------------------------
// WriteVariable — unchanged in behaviour from the original
//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
inline StatusCode XmlFileWriter::WriteVariable(const std::string &xmlKey, const T &t)
{
    if (!m_pCurrentXmlElement)
        return STATUS_CODE_FAILURE;

    TiXmlElement *const pTiXmlElement = new TiXmlElement(xmlKey);
    pTiXmlElement->LinkEndChild(new TiXmlText(TypeToStringPrecision(t)));
    m_pCurrentXmlElement->LinkEndChild(pTiXmlElement);

    return STATUS_CODE_SUCCESS;
}

template <>
inline StatusCode XmlFileWriter::WriteVariable(const std::string &xmlKey, const CartesianVector &t)
{
    return this->WriteVariable(xmlKey,
        TypeToStringPrecision(t.GetX()) + " " +
        TypeToStringPrecision(t.GetY()) + " " +
        TypeToStringPrecision(t.GetZ()));
}

template <>
inline StatusCode XmlFileWriter::WriteVariable(const std::string &xmlKey, const TrackState &t)
{
    return this->WriteVariable(xmlKey,
        TypeToStringPrecision(t.GetPosition().GetX()) + " " +
        TypeToStringPrecision(t.GetPosition().GetY()) + " " +
        TypeToStringPrecision(t.GetPosition().GetZ()) + " " +
        TypeToStringPrecision(t.GetMomentum().GetX()) + " " +
        TypeToStringPrecision(t.GetMomentum().GetY()) + " " +
        TypeToStringPrecision(t.GetMomentum().GetZ()));
}

} // namespace pandora

#endif // #ifndef PANDORA_XML_FILE_WRITER_H
