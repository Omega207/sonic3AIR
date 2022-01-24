/*
*	Part of the Oxygen Engine / Sonic 3 A.I.R. software distribution.
*	Copyright (C) 2017-2022 by Eukaryot
*
*	Published under the GNU GPLv3 open source software license, see license.txt
*	or https://www.gnu.org/licenses/gpl-3.0.en.html
*/

#include "lemon/pch.h"
#include "lemon/program/Function.h"
#include "lemon/program/Module.h"
#include "lemon/runtime/Runtime.h"
#include "lemon/compiler/Utility.h"


namespace lemon
{
	namespace detail
	{
		uint32 getVoidSignatureHash()
		{
			uint32 value = PredefinedDataTypes::VOID.getDataTypeHash();
			return rmx::getFNV1a_32((const uint8*)&value, sizeof(uint32));
		}
	}


	void Function::setParametersByTypes(const std::vector<const DataTypeDefinition*>& parameterTypes)
	{
		mParameters.clear();
		mParameters.resize(parameterTypes.size());
		for (size_t i = 0; i < parameterTypes.size(); ++i)
		{
			mParameters[i].mType = parameterTypes[i];
		}
		mSignatureHash = 0;
	}

	uint32 Function::getVoidSignatureHash()
	{
		static const uint32 signatureHash = detail::getVoidSignatureHash();
		return signatureHash;
	}

	uint32 Function::getSignatureHash() const
	{
		if (mSignatureHash == 0)
		{
			static std::vector<uint32> data;
			data.clear();
			data.push_back(mReturnType->getDataTypeHash());
			for (const Parameter& parameter : mParameters)
			{
				data.push_back(parameter.mType->getDataTypeHash());
			}

			mSignatureHash = rmx::getFNV1a_32((const uint8*)&data[0], data.size() * sizeof(uint32));
			while (mSignatureHash == 0)		// That should be a really rare case anyway
			{
				data.push_back(0xcd000000);		// Just add anything to get away from hash 0
				mSignatureHash = rmx::getFNV1a_32((const uint8*)&data[0], data.size() * sizeof(uint32));
			}
		}
		return mSignatureHash;
	}


	ScriptFunction::~ScriptFunction()
	{
		for (LocalVariable* variable : mLocalVariablesById)
		{
			mModule->destroyLocalVariable(*variable);
		}
	}

	LocalVariable* ScriptFunction::getLocalVariableByIdentifier(const std::string& identifier) const
	{
		const auto it = mLocalVariablesByIdentifier.find(identifier);
		return (it == mLocalVariablesByIdentifier.end()) ? nullptr : it->second;
	}

	LocalVariable& ScriptFunction::getLocalVariableById(uint32 id) const
	{
		return *mLocalVariablesById[id];
	}

	LocalVariable& ScriptFunction::addLocalVariable(const std::string& identifier, const DataTypeDefinition* dataType, uint32 lineNumber)
	{
		// Check if it already exists!
		if (mLocalVariablesByIdentifier.count(identifier))
		{
			CHECK_ERROR(false, "Variable already exists", lineNumber);
		}

		LocalVariable& variable = mModule->createLocalVariable();
		variable.mName = identifier;
		variable.mDataType = dataType;

		mLocalVariablesByIdentifier.emplace(identifier, &variable);

		variable.mId = (uint32)mLocalVariablesById.size();
		mLocalVariablesById.emplace_back(&variable);

		return variable;
	}

	bool ScriptFunction::getLabel(const std::string& labelName, size_t& outOffset) const
	{
		const auto it = mLabels.find(labelName);
		if (it == mLabels.end())
			return false;

		outOffset = it->second;
		return true;
	}

	void ScriptFunction::addLabel(const std::string& labelName, size_t offset)
	{
		mLabels[labelName] = (uint32)offset;
	}

	const std::string* ScriptFunction::findLabelByOffset(size_t offset) const
	{
		// Note that this won't handle multipe labels at the same position too well
		for (const auto& pair : mLabels)
		{
			if (pair.second == offset)
			{
				return &pair.first;
			}
		}
		return nullptr;
	}


	void UserDefinedFunction::setFunction(const FunctionWrapper& functionWrapper)
	{
		mFunctionWrapper = &functionWrapper;
		mReturnType = functionWrapper.getReturnType();
		setParametersByTypes(functionWrapper.getParameterTypes());
	}

	UserDefinedFunction& UserDefinedFunction::setParameterInfo(size_t index, const std::string& identifier)
	{
		RMX_ASSERT(index < mParameters.size(), "Invalid parameter index " << index);
		RMX_ASSERT(mParameters[index].mIdentifier.empty(), "Parameter identifier is already set for index " << index);
		mParameters[index].mIdentifier = identifier;
		return *this;
	}

	void UserDefinedFunction::execute(const Context context) const
	{
		RuntimeDetailHandler* runtimeDetailHandler = context.mControlFlow.getRuntime().getRuntimeDetailHandler();
		if (nullptr != runtimeDetailHandler)
		{
			runtimeDetailHandler->preExecuteExternalFunction(*this, context.mControlFlow);
			mFunctionWrapper->execute(context);
			runtimeDetailHandler->postExecuteExternalFunction(*this, context.mControlFlow);
		}
		else
		{
			mFunctionWrapper->execute(context);
		}
	}

}
