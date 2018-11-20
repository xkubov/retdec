/**
 * @file src/bin2llvmir/providers/config.cpp
 * @brief Config DB provider for bin2llvmirl.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include <iostream>

#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Module.h>

#include "retdec/bin2llvmir/providers/asm_instruction.h"
#include "retdec/bin2llvmir/providers/config.h"
#include "retdec/bin2llvmir/providers/demangler.h"
#include "retdec/bin2llvmir/utils/debug.h"
#include "retdec/bin2llvmir/utils/llvm.h"

using namespace retdec::utils;
using namespace llvm;

namespace retdec {
namespace bin2llvmir {

//
//=============================================================================
//  Config
//=============================================================================
//

Config Config::empty(llvm::Module* m)
{
	Config config;
	config._module = m;
	return config;
}

Config Config::fromFile(llvm::Module* m, const std::string& path)
{
	Config config;
	config._module = m;
	config._configPath = path;
	if (!config._configPath.empty())
	{
		// Can throw an exception, but it is catched by bin2llvmirl.
		config._configDB.readJsonFile(config._configPath);
	}

	for (auto& s : config.getConfig().structures)
	{
		llvm_utils::stringToLlvmType(m->getContext(), s.getLlvmIr());
	}

	// TODO: needed?
	if (config.getConfig().tools.isPic32())
	{
		config.getConfig().architecture.setIsPic32();
	}

	return config;
}

Config Config::fromJsonString(llvm::Module* m, const std::string& json)
{
	Config config;
	config._module = m;
	config._configDB.readJsonString(json);

	for (auto& s : config.getConfig().structures)
	{
		llvm_utils::stringToLlvmType(m->getContext(), s.getLlvmIr());
	}

	// TODO: needed?
	if (config.getConfig().tools.isPic32())
	{
		config.getConfig().architecture.setIsPic32();
	}

	return config;
}

/**
 * Save the config to reflect changes that have been done to it in
 * the bin2llvmirl.
 */
void Config::doFinalization()
{
	tagFunctionsWithUsedCryptoGlobals();

	if (!_configPath.empty())
	{
		_configDB.generateJsonFile(_configPath);
	}
}

retdec::config::Config& Config::getConfig()
{
	return _configDB;
}

const retdec::config::Config& Config::getConfig() const
{
	return _configDB;
}

llvm::Function* Config::getLlvmFunction(Address startAddr)
{
	auto fnc = getConfigFunction(startAddr);
	return fnc ? _module->getFunction(fnc->getName()) : nullptr;
}

retdec::utils::Address Config::getFunctionAddress(
		const llvm::Function* fnc)
{
	retdec::config::Function* cf = getConfigFunction(fnc);
	return cf ? cf->getStart() : retdec::utils::Address();
}

retdec::config::Function* Config::getConfigFunction(
		const llvm::Function* fnc)
{
	return fnc ? _configDB.functions.getFunctionByName(fnc->getName()) : nullptr;
}

retdec::config::Function* Config::getConfigFunction(
		retdec::utils::Address startAddr)
{
	return _configDB.functions.getFunctionByStartAddress(startAddr);
}

llvm::Function* Config::getIntrinsicFunction(IntrinsicFunctionCreatorPtr f)
{
	auto fit = _intrinsicFunctions.find(f);
	if (fit != _intrinsicFunctions.end())
	{
		return fit->second;
	}
	else
	{
		auto* intrinsic = f(_module);
		_intrinsicFunctions[f] = intrinsic;
		return intrinsic;
	}
}

const retdec::config::Object* Config::getConfigGlobalVariable(
		const llvm::GlobalVariable* gv)
{
	return gv ? _configDB.globals.getObjectByName(gv->getName()) : nullptr;
}

const retdec::config::Object* Config::getConfigGlobalVariable(
		retdec::utils::Address address)
{
	return _configDB.globals.getObjectByAddress(address);
}

llvm::GlobalVariable* Config::getLlvmGlobalVariable(Address address)
{
	auto glob = _configDB.globals.getObjectByAddress(address);
	return glob ? _module->getGlobalVariable(glob->getName()) : nullptr;
}

/**
 * Global variables have two unique IDs -- name and address. This method returns
 * LLVM global with either @a name (string search in module's globals) or
 * @a address (config used to get global by address).
 */
llvm::GlobalVariable* Config::getLlvmGlobalVariable(
		const std::string& name,
		retdec::utils::Address address)
{
	if (auto* gv = _module->getGlobalVariable(name))
	{
		return gv;
	}
	else if (auto* gv = getLlvmGlobalVariable(address))
	{
		return gv;
	}
	else
	{
		return nullptr;
	}
}

retdec::utils::Address Config::getGlobalAddress(
		const llvm::GlobalVariable* gv)
{
	assert(gv);
	auto* cgv = gv ? _configDB.globals.getObjectByName(gv->getName()) : nullptr;
	return cgv ? cgv->getStorage().getAddress() : retdec::utils::Address();
}

bool Config::isGlobalVariable(const llvm::Value* val)
{
	auto* gv = dyn_cast<GlobalVariable>(val);
	return getConfigGlobalVariable(gv) != nullptr;
}

const retdec::config::Object* Config::getConfigLocalVariable(
		const llvm::Value* val)
{
	auto* a = dyn_cast_or_null<AllocaInst>(val);
	if (a == nullptr)
	{
		return nullptr;
	}
	auto* cf = getConfigFunction(a->getFunction());
	if (cf == nullptr)
	{
		return nullptr;
	}
	auto* cl = cf->locals.getObjectByName(a->getName());
	return cl && cl->getStorage().isUndefined() ? cl : nullptr;
}

retdec::config::Object* Config::getConfigStackVariable(
		const llvm::Value* val)
{
	auto* a = dyn_cast_or_null<AllocaInst>(val);
	if (a == nullptr)
	{
		return nullptr;
	}
	auto* cf = getConfigFunction(a->getFunction());
	if (cf == nullptr)
	{
		return nullptr;
	}
	auto* cl = const_cast<retdec::config::Object*>(
			cf->locals.getObjectByName(a->getName()));
	return cl && cl->getStorage().isStack() ? cl : nullptr;
}

/**
 * @return LLVM alloca instruction for stack variable with offset @a offset in
 *         function @a fnc. @c nullptr if such variable does not exist.
 */
llvm::AllocaInst* Config::getLlvmStackVariable(
		llvm::Function* fnc,
		int offset)
{
	auto* cf = getConfigFunction(fnc);
	if (cf == nullptr)
	{
		return nullptr;
	}

	for (auto& p: cf->locals)
	{
		auto& l = p.second;
		int off = 0;
		if (l.getStorage().isStack(off) && off == offset)
		{
			for (auto& b : *fnc)
			for (auto& i : b)
			{
				if (AllocaInst* a = dyn_cast<AllocaInst>(&i))
				{
					if (a->getName() == l.getName())
					{
						return a;
					}
				}
			}
		}
	}

	return nullptr;
}

/**
 * @return @c True if the the provided LLVM value @a val is a stack variable.
 *         @c False otherwise.
 */
bool Config::isStackVariable(const llvm::Value* val)
{
	return getConfigStackVariable(val) != nullptr;
}

retdec::utils::Maybe<int> Config::getStackVariableOffset(
		const llvm::Value* val)
{
	auto* sv = getConfigStackVariable(val);
	return sv
			? retdec::utils::Maybe<int>(sv->getStorage().getStackOffset())
			: retdec::utils::Maybe<int>();
}

retdec::config::Object* Config::insertGlobalVariable(
		const llvm::GlobalVariable* gv,
		retdec::utils::Address address,
		bool fromDebug,
		const std::string& realName,
		const std::string& cryptoDesc)
{
	retdec::config::Object cgv(
			gv->getName(),
			retdec::config::Storage::inMemory(address));
	cgv.setIsFromDebug(fromDebug);
	cgv.setRealName(realName);
	cgv.setCryptoDescription(cryptoDesc);
	cgv.type.setLlvmIr(llvmObjToString(gv->getType()->getElementType()));
	if (gv->hasInitializer() && gv->getInitializer()->getName() == "wide-string")
	{
		cgv.type.setIsWideString(true);
	}
	auto p = _configDB.globals.insert(cgv);
	return &p.first->second;
}

retdec::config::Object* Config::insertStackVariable(
		const llvm::AllocaInst* sv,
		int offset,
		bool fromDebug)
{
	auto* cf = getConfigFunction(sv->getFunction());

	if (cf == nullptr)
	{
		assert(false);
	}

	assert(cf);
	if (cf == nullptr)
	{
		return nullptr;
	}

	retdec::config::Object local(
			sv->getName(),
			retdec::config::Storage::onStack(offset));
	local.setRealName(sv->getName());
	local.setIsFromDebug(fromDebug);
	local.type.setLlvmIr(llvmObjToString(sv->getType()));

	auto p = cf->locals.insert(local);
	return &p.first->second;
}

retdec::config::Function* Config::insertFunction(
		const llvm::Function* fnc,
		retdec::utils::Address start,
		retdec::utils::Address end,
		bool fromDebug)
{
	std::string dm;
	if (auto* old = getConfigFunction(start))
	{
		dm = old->getDemangledName();
	}

	retdec::config::Function cf(fnc->getName());
	cf.setDemangledName(dm);
	cf.setIsFromDebug(fromDebug);
	cf.setStartEnd(start, end);

	if (cf.getDemangledName().empty())
	{
		retdec::demangler::CDemangler* d = DemanglerProvider::getDemangler(_module);
		if (d)
		{
			auto s = d->demangleToString(fnc->getName());
			if (!s.empty())
			{
				cf.setDemangledName(s);
			}
		}
	}

	auto p = _configDB.functions.insert(cf);
	return &p.first->second;
}

retdec::config::Function* Config::renameFunction(
		retdec::config::Function* fnc,
		const std::string& name)
{
	retdec::config::Function cf = *fnc;
	cf.setName(name);

	if (cf.getDemangledName().empty())
	{
		retdec::demangler::CDemangler* d = DemanglerProvider::getDemangler(_module);
		if (d)
		{
			auto s = d->demangleToString(fnc->getName());
			if (!s.empty())
			{
				cf.setDemangledName(s);
			}
		}
	}

	_configDB.functions.erase(fnc->getName());
	auto p = _configDB.functions.insert(cf);
	return &p.first->second;
}

const retdec::config::Object* Config::getConfigRegister(
		const llvm::Value* val)
{
	auto* gv = dyn_cast_or_null<GlobalVariable>(val);
	return gv ? _configDB.registers.getObjectByName(gv->getName()) : nullptr;
}

retdec::utils::Maybe<unsigned> Config::getConfigRegisterNumber(
		const llvm::Value* val)
{
	retdec::utils::Maybe<unsigned> undefVal;
	auto* r = getConfigRegister(val);
	return r ? r->getStorage().getRegisterNumber() : undefVal;
}

llvm::GlobalVariable* Config::getLlvmRegister(
		const std::string& name)
{
	auto* cr = _configDB.registers.getObjectByRealName(name);
	return cr ? _module->getNamedGlobal(cr->getName()) : nullptr;
}

/**
 * @return Always returns the same dummy global variable.
 */
llvm::GlobalVariable* Config::getGlobalDummy()
{
	if (_globalDummy == nullptr)
	{
		_globalDummy = new GlobalVariable(
				*_module,
				Type::getInt32Ty(_module->getContext()),
				false,
				GlobalValue::ExternalLinkage,
				nullptr);
		assert(_globalDummy);
	}
	return _globalDummy;
}

utils::FilesystemPath Config::getOutputDirectory()
{
	FilesystemPath fsp(getConfig().parameters.getOutputFile());
	return fsp.getParentPath();
}

void Config::setLlvmCallPseudoFunction(llvm::Function* f)
{
	_callFunction = f;
}
llvm::Function* Config::getLlvmCallPseudoFunction() const
{
	return _callFunction;
}
bool Config::isLlvmCallPseudoFunction(llvm::Value* f)
{
	return _callFunction == f;
}
llvm::CallInst* Config::isLlvmCallPseudoFunctionCall(llvm::Value* c)
{
	auto* cc = dyn_cast_or_null<CallInst>(c);
	return cc && cc->getCalledValue() == _callFunction ? cc : nullptr;
}

void Config::setLlvmReturnPseudoFunction(llvm::Function* f)
{
	_returnFunction = f;
}
llvm::Function* Config::getLlvmReturnPseudoFunction() const
{
	return _returnFunction;
}
bool Config::isLlvmReturnPseudoFunction(llvm::Value* f)
{
	return _returnFunction == f;
}
llvm::CallInst* Config::isLlvmReturnPseudoFunctionCall(llvm::Value* c)
{
	auto* cc = dyn_cast_or_null<CallInst>(c);
	return cc && cc->getCalledValue() == _returnFunction ? cc : nullptr;
}

void Config::setLlvmBranchPseudoFunction(llvm::Function* f)
{
	_branchFunction = f;
}
llvm::Function* Config::getLlvmBranchPseudoFunction() const
{
	return _branchFunction;
}
bool Config::isLlvmBranchPseudoFunction(llvm::Value* f)
{
	return _branchFunction == f;
}
llvm::CallInst* Config::isLlvmBranchPseudoFunctionCall(llvm::Value* c)
{
	auto* cc = dyn_cast_or_null<CallInst>(c);
	return cc && cc->getCalledValue() == _branchFunction ? cc : nullptr;
}

void Config::setLlvmCondBranchPseudoFunction(llvm::Function* f)
{
	_condBranchFunction = f;
}
llvm::Function* Config::getLlvmCondBranchPseudoFunction() const
{
	return _condBranchFunction;
}
bool Config::isLlvmCondBranchPseudoFunction(llvm::Value* f)
{
	return _condBranchFunction == f;
}
llvm::CallInst* Config::isLlvmCondBranchPseudoFunctionCall(llvm::Value* c)
{
	auto* cc = dyn_cast_or_null<CallInst>(c);
	return cc && cc->getCalledValue() == _condBranchFunction ? cc : nullptr;
}

void Config::setLlvmX87DataStorePseudoFunction(llvm::Function* f)
{
	_x87DataStoreFunction = f;
}
llvm::Function* Config::getLlvmX87DataStorePseudoFunction() const
{
	return _x87DataStoreFunction;
}
bool Config::isLlvmX87DataStorePseudoFunction(llvm::Value* f)
{
	return _x87DataStoreFunction == f;
}
llvm::CallInst* Config::isLlvmX87DataStorePseudoFunctionCall(llvm::Value* c)
{
	auto* cc = dyn_cast_or_null<CallInst>(c);
	return cc && cc->getCalledValue() == _x87DataStoreFunction ? cc : nullptr;
}

void Config::setLlvmX87TagStorePseudoFunction(llvm::Function* f)
{
	_x87TagStoreFunction = f;
}
llvm::Function* Config::getLlvmX87TagStorePseudoFunction() const
{
	return _x87TagStoreFunction;
}
bool Config::isLlvmX87TagStorePseudoFunction(llvm::Value* f)
{
	return _x87TagStoreFunction == f;
}
llvm::CallInst* Config::isLlvmX87TagStorePseudoFunctionCall(llvm::Value* c)
{
	auto* cc = dyn_cast_or_null<CallInst>(c);
	return cc && cc->getCalledValue() == _x87TagStoreFunction ? cc : nullptr;
}

void Config::setLlvmX87DataLoadPseudoFunction(llvm::Function* f)
{
	_x87DataLoadFunction = f;
}
llvm::Function* Config::getLlvmX87DataLoadPseudoFunction() const
{
	return _x87DataLoadFunction;
}
bool Config::isLlvmX87DataLoadPseudoFunction(llvm::Value* f)
{
	return _x87DataLoadFunction == f;
}
llvm::CallInst* Config::isLlvmX87DataLoadPseudoFunctionCall(llvm::Value* c)
{
	auto* cc = dyn_cast_or_null<CallInst>(c);
	return cc && cc->getCalledValue() == _x87DataLoadFunction ? cc : nullptr;
}

void Config::setLlvmX87TagLoadPseudoFunction(llvm::Function* f)
{
	_x87TagLoadFunction = f;
}
llvm::Function* Config::getLlvmX87TagLoadPseudoFunction() const
{
	return _x87TagLoadFunction;
}
bool Config::isLlvmX87TagLoadPseudoFunction(llvm::Value* f)
{
	return _x87TagLoadFunction == f;
}
llvm::CallInst* Config::isLlvmX87TagLoadPseudoFunctionCall(llvm::Value* c)
{
	auto* cc = dyn_cast_or_null<CallInst>(c);
	return cc && cc->getCalledValue() == _x87TagLoadFunction ? cc : nullptr;
}

llvm::CallInst* Config::isLlvmX87StorePseudoFunctionCall(llvm::Value* c)
{
	if (auto* cc = isLlvmX87DataStorePseudoFunctionCall(c)) return cc;
	if (auto* cc = isLlvmX87TagStorePseudoFunctionCall(c)) return cc;
	return nullptr;
}

llvm::CallInst* Config::isLlvmX87LoadPseudoFunctionCall(llvm::Value* c)
{
	if (auto* cc = isLlvmX87DataLoadPseudoFunctionCall(c)) return cc;
	if (auto* cc = isLlvmX87TagLoadPseudoFunctionCall(c)) return cc;
	return nullptr;
}

llvm::CallInst* Config::isLlvmAnyBranchPseudoFunctionCall(llvm::Value* c)
{
	if (auto* cc = isLlvmCallPseudoFunctionCall(c)) return cc;
	if (auto* cc = isLlvmReturnPseudoFunctionCall(c)) return cc;
	if (auto* cc = isLlvmBranchPseudoFunctionCall(c)) return cc;
	if (auto* cc = isLlvmCondBranchPseudoFunctionCall(c)) return cc;
	return nullptr;
}

llvm::CallInst* Config::isLlvmAnyUncondBranchPseudoFunctionCall(llvm::Value* c)
{
	if (auto* cc = isLlvmCallPseudoFunctionCall(c)) return cc;
	if (auto* cc = isLlvmReturnPseudoFunctionCall(c)) return cc;
	if (auto* cc = isLlvmBranchPseudoFunctionCall(c)) return cc;
	return nullptr;
}

void Config::addPseudoAsmFunction(llvm::Function* f)
{
	_pseudoAsmFunctions.insert(f);
}

bool Config::isPseudoAsmFunction(llvm::Function* f)
{
	return _pseudoAsmFunctions.count(f);
}

llvm::CallInst* Config::isPseudoAsmFunctionCall(llvm::Value* c)
{
	auto* cc = dyn_cast_or_null<CallInst>(c);
	return isPseudoAsmFunction(cc->getCalledFunction()) ? cc : nullptr;
}

/**
 * Get crypto pattern information for address \p addr - fill \p name,
 * \p description, and \p type, if there is a pattern on address.
 * \return \c True if pattern was found, \c false otherwise.
 */
bool Config::getCryptoPattern(
		retdec::utils::Address addr,
		std::string& name,
		std::string& description,
		llvm::Type*& type) const
{
	for (auto& p : getConfig().patterns)
	{
		if (!p.isTypeCrypto())
		{
			continue;
		}

		for (auto& m : p.matches)
		{
			if (m.getAddress() != addr || !m.isSizeDefined())
			{
				continue;
			}

			unsigned elemCount = m.getSize();
			Type* elemType = Type::getInt8Ty(_module->getContext());

			if (m.isEntrySizeDefined())
			{
				elemCount /= m.getEntrySize();
				if (m.isTypeFloatingPoint() && m.getEntrySize() == 8)
				{
					elemType = Type::getDoubleTy(_module->getContext());
				}
				else if (m.isTypeFloatingPoint() && m.getEntrySize() == 2)
				{
					elemType = Type::getHalfTy(_module->getContext());
				}
				else if (m.isTypeFloatingPoint() && m.getEntrySize() == 10)
				{
					elemType = Type::getX86_FP80Ty(_module->getContext());
				}
				else if (m.isTypeFloatingPoint())
				{
					elemType = Type::getFloatTy(_module->getContext());
				}
				else // integral || unknown
				{
					elemType = Type::getIntNTy(
							_module->getContext(),
							m.getEntrySize() * 8);
				}
			}
			auto d = elemCount > 0 ? elemCount : 1;
			type = ArrayType::get(elemType, d);
			name = retdec::utils::appendHexRet(p.getName() + "_at", addr);
			description = p.getDescription();

			return true;
		}
	}

	return false;
}

void Config::tagFunctionsWithUsedCryptoGlobals()
{
	for (GlobalVariable& lgv : _module->getGlobalList())
	{
		auto* cgv = getConfigGlobalVariable(&lgv);
		if (cgv == nullptr || cgv->getCryptoDescription().empty())
		{
			continue;
		}

		for (auto* user : lgv.users())
		{
			if (auto* i = dyn_cast_or_null<Instruction>(user))
			{
				if (auto* cf = getConfigFunction(i->getFunction()))
				{
					cf->usedCryptoConstants.insert(cgv->getCryptoDescription());
				}
			}
			else if (auto* e = dyn_cast_or_null<ConstantExpr>(user))
			{
				for (auto* u : e->users())
				{
					if (auto* i = dyn_cast_or_null<Instruction>(u))
					{
						if (auto* cf = getConfigFunction(i->getFunction()))
						{
							cf->usedCryptoConstants.insert(cgv->getCryptoDescription());
						}
					}
				}
			}
		}
	}
}

//
//=============================================================================
//  ConfigProvider
//=============================================================================
//

std::map<llvm::Module*, Config> ConfigProvider::_module2config;

Config* ConfigProvider::addConfigFile(llvm::Module* m, const std::string& path)
{
	auto p = _module2config.emplace(m, Config::fromFile(m, path));
	return &p.first->second;
}

Config* ConfigProvider::addConfigJsonString(
		llvm::Module* m,
		const std::string& json)
{
	auto p = _module2config.emplace(m, Config::fromJsonString(m, json));
	return &p.first->second;
}

Config* ConfigProvider::getConfig(llvm::Module* m)
{
	auto f = _module2config.find(m);
	return f != _module2config.end() ? &f->second : nullptr;
}

bool ConfigProvider::getConfig(llvm::Module* m, Config*& c)
{
	c = getConfig(m);
	return c != nullptr;
}

void ConfigProvider::doFinalization(llvm::Module* m)
{
	auto* c = getConfig(m);
	if (c)
	{
		c->doFinalization();
	}
}

/**
 * Clear all stored data.
 */
void ConfigProvider::clear()
{
	_module2config.clear();
}

} // namespace bin2llvmir
} // namespace retdec
