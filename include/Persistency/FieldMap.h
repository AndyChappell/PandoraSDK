/**
 *  @file   PandoraSDK/include/Persistency/FieldMap.h
 *
 *  @brief  Header file for the FieldMap class used for self-describing,
 *          forward/backward-compatible component serialisation.
 *
 *  $Log: $
 */
#ifndef PANDORA_FIELD_MAP_H
#define PANDORA_FIELD_MAP_H 1

#include "Pandora/StatusCodes.h"

#include "Objects/CartesianVector.h"
#include "Objects/TrackState.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace pandora
{

//------------------------------------------------------------------------------------------------------------------------------------------

/**
 *  @brief  FieldMap
 *
 *  A flat key/value store that holds all serialised fields for a single component,
 *  keyed by their string tag names. This is the central abstraction that decouples
 *  the file format (binary / XML) from the component deserialisation logic.
 *
 *  Design contract
 *  ---------------
 *  - Writers populate a FieldMap by calling Set<T>(tag, value) for every field.
 *  - Readers call Get<T>(tag, value) to retrieve fields by name.
 *  - If a field is absent (old file, new reader), Get returns STATUS_CODE_NOT_FOUND
 *    and the caller supplies its own default — there is no silent data corruption.
 *  - If a field is present but its stored byte count does not match sizeof(T), Get
 *    returns STATUS_CODE_INVALID_PARAMETER so schema mismatches are always visible.
 *  - Unknown fields encountered in a file (new file, old reader) are stored in the
 *    map but never queried — they are simply discarded at end of scope.
 *
 *  Supported value types
 *  ---------------------
 *  Any trivially-copyable T is supported by the generic Get/Set pair.
 *  Specialisations are provided for std::string, CartesianVector, and TrackState
 *  so that these compound types serialise consistently with the binary wire format.
 *
 *  The raw byte accessors (GetRawBytes / SetRawBytes) are used by format-specific
 *  readers and writers; component code should always use the typed accessors.
 */
class FieldMap
{
public:
    template <typename T>
    void Set(const std::string &tag, const T &value);

    template <typename T>
    StatusCode Get(const std::string &tag, T &value) const;

    template <typename T>
    T GetOrDefault(const std::string &tag, const T &defaultValue) const;

    bool Has(const std::string &tag) const;
    void SetRawBytes(const std::string &tag, std::vector<unsigned char> bytes);
    StatusCode GetRawBytes(const std::string &tag, std::vector<unsigned char> &bytes) const;
    void Remove(const std::string &tag);
    const std::unordered_map<std::string, std::vector<unsigned char>> &GetAllFields() const;

private:
    std::unordered_map<std::string, std::vector<unsigned char>> m_fields;
};

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
inline void FieldMap::Set(const std::string &tag, const T &value)
{
    static_assert(std::is_trivially_copyable<T>::value,
        "FieldMap::Set requires trivially-copyable T; provide a specialisation for compound types");
    std::vector<unsigned char> bytes(sizeof(T));
    std::memcpy(bytes.data(), &value, sizeof(T));
    m_fields[tag] = std::move(bytes);
}

template <typename T>
inline StatusCode FieldMap::Get(const std::string &tag, T &value) const
{
    static_assert(std::is_trivially_copyable<T>::value,
        "FieldMap::Get requires trivially-copyable T; provide a specialisation for compound types");
    auto it = m_fields.find(tag);
    if (it == m_fields.end()) return STATUS_CODE_NOT_FOUND;
    if (it->second.size() != sizeof(T)) return STATUS_CODE_INVALID_PARAMETER;
    std::memcpy(&value, it->second.data(), sizeof(T));
    return STATUS_CODE_SUCCESS;
}

template <typename T>
inline T FieldMap::GetOrDefault(const std::string &tag, const T &defaultValue) const
{
    // Check presence before attempting construction of T, so that types
    // without a default constructor (e.g. CartesianVector, TrackState)
    // are never default-constructed — we return defaultValue directly.
    auto it = m_fields.find(tag);

    if (it == m_fields.end())
        return defaultValue;

    T value{defaultValue};   // copy-construct from default so compound types
                             // are always in a valid state before Get overwrites
    const StatusCode sc = this->Get(tag, value);

    if (STATUS_CODE_SUCCESS != sc)
        throw StatusCodeException(sc);

    return value;
}

inline bool FieldMap::Has(const std::string &tag) const
    { return m_fields.count(tag) > 0; }

inline void FieldMap::SetRawBytes(const std::string &tag, std::vector<unsigned char> bytes)
    { m_fields[tag] = std::move(bytes); }

inline StatusCode FieldMap::GetRawBytes(const std::string &tag, std::vector<unsigned char> &bytes) const
{
    auto it = m_fields.find(tag);
    if (it == m_fields.end()) return STATUS_CODE_NOT_FOUND;
    bytes = it->second;
    return STATUS_CODE_SUCCESS;
}

inline void FieldMap::Remove(const std::string &tag) { m_fields.erase(tag); }

inline const std::unordered_map<std::string, std::vector<unsigned char>> &FieldMap::GetAllFields() const
    { return m_fields; }

//------------------------------------------------------------------------------------------------------------------------------------------
// Specialisations for compound Pandora types
//------------------------------------------------------------------------------------------------------------------------------------------

template <>
inline void FieldMap::Set(const std::string &tag, const std::string &value)
{
    const uint32_t length = static_cast<uint32_t>(value.size());
    std::vector<unsigned char> bytes(sizeof(uint32_t) + length);
    std::memcpy(bytes.data(), &length, sizeof(uint32_t));
    if (length > 0) std::memcpy(bytes.data() + sizeof(uint32_t), value.data(), length);
    m_fields[tag] = std::move(bytes);
}

template <>
inline StatusCode FieldMap::Get(const std::string &tag, std::string &value) const
{
    auto it = m_fields.find(tag);
    if (it == m_fields.end()) return STATUS_CODE_NOT_FOUND;
    const std::vector<unsigned char> &bytes = it->second;
    if (bytes.size() < sizeof(uint32_t)) return STATUS_CODE_INVALID_PARAMETER;
    uint32_t length = 0;
    std::memcpy(&length, bytes.data(), sizeof(uint32_t));
    if (bytes.size() != sizeof(uint32_t) + length) return STATUS_CODE_INVALID_PARAMETER;
    value.assign(reinterpret_cast<const char *>(bytes.data() + sizeof(uint32_t)), length);
    return STATUS_CODE_SUCCESS;
}

template <>
inline void FieldMap::Set(const std::string &tag, const CartesianVector &value)
{
    const float x = value.GetX(), y = value.GetY(), z = value.GetZ();
    std::vector<unsigned char> bytes(3 * sizeof(float));
    std::memcpy(bytes.data() + 0 * sizeof(float), &x, sizeof(float));
    std::memcpy(bytes.data() + 1 * sizeof(float), &y, sizeof(float));
    std::memcpy(bytes.data() + 2 * sizeof(float), &z, sizeof(float));
    m_fields[tag] = std::move(bytes);
}

template <>
inline StatusCode FieldMap::Get(const std::string &tag, CartesianVector &value) const
{
    auto it = m_fields.find(tag);
    if (it == m_fields.end()) return STATUS_CODE_NOT_FOUND;
    if (it->second.size() != 3 * sizeof(float)) return STATUS_CODE_INVALID_PARAMETER;
    float x = 0.f, y = 0.f, z = 0.f;
    std::memcpy(&x, it->second.data() + 0 * sizeof(float), sizeof(float));
    std::memcpy(&y, it->second.data() + 1 * sizeof(float), sizeof(float));
    std::memcpy(&z, it->second.data() + 2 * sizeof(float), sizeof(float));
    value = CartesianVector(x, y, z);
    return STATUS_CODE_SUCCESS;
}

template <>
inline void FieldMap::Set(const std::string &tag, const TrackState &value)
{
    const float px = value.GetPosition().GetX(), py = value.GetPosition().GetY(), pz = value.GetPosition().GetZ();
    const float mx = value.GetMomentum().GetX(), my = value.GetMomentum().GetY(), mz = value.GetMomentum().GetZ();
    std::vector<unsigned char> bytes(6 * sizeof(float));
    std::memcpy(bytes.data() + 0 * sizeof(float), &px, sizeof(float));
    std::memcpy(bytes.data() + 1 * sizeof(float), &py, sizeof(float));
    std::memcpy(bytes.data() + 2 * sizeof(float), &pz, sizeof(float));
    std::memcpy(bytes.data() + 3 * sizeof(float), &mx, sizeof(float));
    std::memcpy(bytes.data() + 4 * sizeof(float), &my, sizeof(float));
    std::memcpy(bytes.data() + 5 * sizeof(float), &mz, sizeof(float));
    m_fields[tag] = std::move(bytes);
}

template <>
inline StatusCode FieldMap::Get(const std::string &tag, TrackState &value) const
{
    auto it = m_fields.find(tag);
    if (it == m_fields.end()) return STATUS_CODE_NOT_FOUND;
    if (it->second.size() != 6 * sizeof(float)) return STATUS_CODE_INVALID_PARAMETER;
    float px = 0.f, py = 0.f, pz = 0.f, mx = 0.f, my = 0.f, mz = 0.f;
    std::memcpy(&px, it->second.data() + 0 * sizeof(float), sizeof(float));
    std::memcpy(&py, it->second.data() + 1 * sizeof(float), sizeof(float));
    std::memcpy(&pz, it->second.data() + 2 * sizeof(float), sizeof(float));
    std::memcpy(&mx, it->second.data() + 3 * sizeof(float), sizeof(float));
    std::memcpy(&my, it->second.data() + 4 * sizeof(float), sizeof(float));
    std::memcpy(&mz, it->second.data() + 5 * sizeof(float), sizeof(float));
    value = TrackState(CartesianVector(px, py, pz), CartesianVector(mx, my, mz));
    return STATUS_CODE_SUCCESS;
}

} // namespace pandora

#endif // #ifndef PANDORA_FIELD_MAP_H
