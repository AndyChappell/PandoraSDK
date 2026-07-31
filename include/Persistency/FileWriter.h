/**
 *  @file   PandoraSDK/include/Persistency/FileWriter.h
 *
 *  @brief  Header file for the file writer class.
 *
 *  $Log: $
 */
#ifndef PANDORA_FILE_WRITER_H
#define PANDORA_FILE_WRITER_H 1

#include "Pandora/StatusCodes.h"

#include "Persistency/PandoraIO.h"
#include "Persistency/Persistency.h"

#include <string>

namespace pandora
{

class Pandora;

//------------------------------------------------------------------------------------------------------------------------------------------

/**
 *  @brief  FileWriter class
 */
class FileWriter : public Persistency
{
public:
    /**
     *  @brief  Constructor
     *
     *  @param  pandora      the pandora instance to be used alongside the file writer
     *  @param  fileName     the name of the output file
     *  @param  majorVersion legacy major version carried into the VERSION_COMPONENT record
     *  @param  minorVersion legacy minor version carried into the VERSION_COMPONENT record
     */
    FileWriter(const pandora::Pandora &pandora, const std::string &fileName,
        const unsigned int majorVersion = 1, const unsigned int minorVersion = 0);

    /**
     *  @brief  Destructor
     */
    virtual ~FileWriter();

    /**
     *  @brief  Write the global header to the file.
     *
     *  The base implementation writes an empty HEADER_CONTAINER containing only
     *  the VERSION_COMPONENT and the container footer. Concrete subclasses that
     *  support the full self-describing format (FileMetadata + SchemaRegistry)
     *  override this and call WriteFooter themselves.
     */
    virtual StatusCode WriteGlobalHeader();

    /**
     *  @brief  Write the current geometry information to the file
     */
    StatusCode WriteGeometry();

    /**
     *  @brief  Write the specified event components to the file.
     *
     *  WriteEventInformation is now always called (no longer gated on a legacy
     *  version number); the component is simply absent in files written by older
     *  readers that do not call this method, and the new reader handles absence
     *  gracefully via FieldMap::GetOrDefault.
     *
     *  @param  caloHitList              the list of calo hits to write
     *  @param  trackList                the list of tracks to write
     *  @param  mcParticleList           the list of MC particles to write
     *  @param  writeMCRelationships     whether to write MC relationship information
     *  @param  writeTrackRelationships  whether to write track relationship information
     */
    StatusCode WriteEvent(const CaloHitList &caloHitList, const TrackList &trackList,
        const MCParticleList &mcParticleList, const bool writeMCRelationships = true,
        const bool writeTrackRelationships = true);

protected:
    /**
     *  @brief  Write the container header to the file
     */
    virtual StatusCode WriteHeader(const ContainerId containerId) = 0;

    /**
     *  @brief  Write the container footer to the file
     */
    virtual StatusCode WriteFooter() = 0;

    /**
     *  @brief  Write the legacy VERSION_COMPONENT record to the file.
     *          Called by the base WriteGlobalHeader; subclasses that override
     *          WriteGlobalHeader call this themselves at the appropriate point.
     */
    virtual StatusCode WriteVersion() = 0;

    virtual StatusCode WriteSubDetector(const SubDetector *const pSubDetector) = 0;
    virtual StatusCode WriteLArTPC(const LArTPC *const pLArTPC) = 0;
    virtual StatusCode WriteDetectorGap(const DetectorGap *const pDetectorGap) = 0;
    virtual StatusCode WriteCaloHit(const CaloHit *const pCaloHit) = 0;
    virtual StatusCode WriteTrack(const Track *const pTrack) = 0;
    virtual StatusCode WriteMCParticle(const MCParticle *const pMCParticle) = 0;

    virtual StatusCode WriteRelationship(const RelationshipId relationshipId,
        const void *address1, const void *address2, const float weight = 1.f) = 0;

    /**
     *  @brief  Write event-level information (run / subrun / event numbers).
     *          Always called by WriteEvent; implementations must tolerate being
     *          called on every event.
     */
    virtual StatusCode WriteEventInformation() = 0;

    unsigned int m_fileMajorVersion; ///< Legacy major version written into VERSION_COMPONENT
    unsigned int m_fileMinorVersion; ///< Legacy minor version written into VERSION_COMPONENT

private:
    StatusCode WriteSubDetectorList();
    StatusCode WriteLArTPCList();
    StatusCode WriteDetectorGapList();
    StatusCode WriteTrackList(const TrackList &trackList);
    StatusCode WriteCaloHitList(const CaloHitList &caloHitList);
    StatusCode WriteMCParticleList(const MCParticleList &mcParticleList);
    StatusCode WriteCaloHitToMCParticleRelationships(const CaloHitList &caloHitList);
    StatusCode WriteTrackToMCParticleRelationships(const TrackList &trackList);
    StatusCode WriteMCParticleRelationships(const MCParticleList &mcParticleList);
    StatusCode WriteTrackRelationships(const TrackList &trackList);
    StatusCode WriteCaloHitToMCParticleRelationship(const CaloHit *const pCaloHit);
    StatusCode WriteTrackToMCParticleRelationship(const Track *const pTrack);
    StatusCode WriteMCParticleRelationships(const MCParticle *const pMCParticle);
    StatusCode WriteTrackRelationships(const Track *const pTrack);
};

} // namespace pandora

#endif // #ifndef PANDORA_FILE_WRITER_H
