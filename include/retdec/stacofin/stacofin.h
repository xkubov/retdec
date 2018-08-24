/**
 * @file include/retdec/stacofin/stacofin.h
 * @brief Static code finder library.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#ifndef RETDEC_STACOFIN_STACOFIN_H
#define RETDEC_STACOFIN_STACOFIN_H

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <capstone/capstone.h>

#include "retdec/config/config.h"
#include "retdec/fileformat/fileformat.h"
#include "retdec/utils/address.h"

namespace retdec {
namespace loader {
	class Image;
} // namespace loader

namespace stacofin {

struct DetectedFunction;

/**
 * Data-type for offset-name relocation pairs.
 */
using CoveredCode = retdec::utils::AddressRangeContainer;

/**
 * Structure representing one reference in a detected function's body.
 */
struct Reference
{
	public:
		Reference(
				std::size_t o,
				const std::string& n,
				utils::Address a = utils::Address::getUndef,
				utils::Address t = utils::Address::getUndef,
				DetectedFunction* tf = nullptr,
				bool k = false);

	public:
		std::size_t offset = 0;
		std::string name;

		utils::Address address;
		utils::Address target;
		DetectedFunction* targetFnc = nullptr;
		bool ok = false;
};

using References = std::vector<Reference>;

/**
 * Structure representing one detected function.
 */
struct DetectedFunction
{
	public:
		bool operator<(const DetectedFunction& o) const;

		bool allRefsOk() const;
		std::size_t countRefsOk() const;
		float refsOkShare() const;
		std::string getName() const;
		bool isTerminating() const;
		bool isThumb() const;

		void setReferences(const std::string &refsString);

		void setAddress(retdec::utils::Address a);
		retdec::utils::Address getAddress() const;

	public:
		/// Original size of source.
		std::size_t size;
		// File offset.
		std::size_t offset;

		/// Possible original names.
		std::vector<std::string> names;
		/// Offset-name relocation pairs.
		References references;

		/// Source signature path.
		std::string signaturePath;

	private:
		/// Virtual address.
		retdec::utils::Address address;
};

using DetectedFunctionsPtrMap = typename std::map<
		utils::Address,
		DetectedFunction*>;
using DetectedFunctionsMultimap = typename std::multimap<
		utils::Address,
		DetectedFunction>;
using DetectedFunctionsPtrMultimap = typename std::multimap<
		utils::Address,
		DetectedFunction*>;

/**
 * Finder implementation using Yara.
 */
class Finder
{
	public:
		Finder();
		~Finder();

		/// @name Actions.
		/// @{
		void clear();
		void search(
				const retdec::loader::Image& image,
				const std::string& yaraFile);
		void search(
				const retdec::loader::Image& image,
				const std::set<std::string>& yaraFiles);
		void search(
				const retdec::loader::Image& image,
				const retdec::config::Config& config);
		/// @}

		/// @name Getters.
		/// @{
		CoveredCode getCoveredCode();
		const std::vector<DetectedFunction>& getDectedFunctions();
		/// @}

	private:
		void sort();

	private:
		/// Code coverage.
		CoveredCode coveredCode;
		/// Functions.
		std::vector<DetectedFunction> detectedFunctions;
		/// @c true if detected functions are sorted.
		bool isSorted = true;

//==============================================================================

	public:
		void searchAndConfirm(
				const retdec::loader::Image& image,
				const retdec::config::Config& config,
				csh ce,
				cs_mode md,
				bool debug = false);

		const retdec::stacofin::DetectedFunctionsMultimap& getAllDetections() const;
		const retdec::stacofin::DetectedFunctionsPtrMap& getConfirmedDetections() const;

	private:
		using ByteData = typename std::pair<const std::uint8_t*, std::size_t>;

	private:
		void solveReferences();

		utils::Address getAddressFromRef(utils::Address ref);
		utils::Address getAddressFromRef_x86(utils::Address ref);
		utils::Address getAddressFromRef_mips(utils::Address ref);
		utils::Address getAddressFromRef_arm(utils::Address ref);
		utils::Address getAddressFromRef_ppc(utils::Address ref);

		void checkRef(retdec::stacofin::Reference& ref);
		void checkRef_x86(retdec::stacofin::Reference& ref);

		void confirmWithoutRefs();
		void confirmAllRefsOk(std::size_t minFncSzWithoutRefs = 0x20);
		void confirmPartialRefsOk(float okShare = 0.5);
		void confirmFunction(retdec::stacofin::DetectedFunction* f);

	private:
		const retdec::config::Config* _config = nullptr;
		const retdec::loader::Image* _image = nullptr;

		csh _ce;
		cs_mode _ceMode;
		cs_insn* _ceInsn = nullptr;

		std::map<utils::Address, std::string> _imports;
		std::set<std::string> _sectionNames;

		retdec::stacofin::DetectedFunctionsMultimap _allDetections;
		retdec::stacofin::DetectedFunctionsPtrMap _confirmedDetections;
		retdec::stacofin::DetectedFunctionsPtrMultimap _rejectedDetections;

	private:
		struct DetectedFunctionComp
		{
			bool operator()(
					const retdec::stacofin::DetectedFunction* a,
					const retdec::stacofin::DetectedFunction* b) const
			{
				return *a < *b;
			}
		};
		std::set<retdec::stacofin::DetectedFunction*, DetectedFunctionComp> _worklistDetections;
};

} // namespace stacofin
} // namespace retdec

#endif
