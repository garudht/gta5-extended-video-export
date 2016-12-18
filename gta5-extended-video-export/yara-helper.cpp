#include "yara-helper.h"
#include "logger.h"

YaraHelper::~YaraHelper()
{
	LOG_CALL(LL_DBG, yr_compiler_destroy(this->pYrCompiler));
	LOG_CALL(LL_DBG, yr_finalize());
}

void YaraHelper::initialize()
{
	PRE();
	this->moduleHandle = ::GetModuleHandle(NULL);
	HANDLE processHandle = ::GetCurrentProcess();
	this->moduleInfo = { 0 };
	LOG(LL_DBG, sizeof(this->moduleInfo.lpBaseOfDll));
	if (GetModuleInformation(processHandle, this->moduleHandle, &this->moduleInfo, sizeof(this->moduleInfo)) != TRUE) {
		DWORD err = ::GetLastError();
		LOG(LL_ERR, "Failed to get module information:", err, " ", Logger::hex(err, 8));
	}


	LOG(LL_NFO, "Process: base:", Logger::hex((uint64_t)this->moduleInfo.lpBaseOfDll, 16), " size:", Logger::hex(this->moduleInfo.SizeOfImage, 16));

	LOG_CALL(LL_DBG, yr_initialize());
	REQUIRE(yr_compiler_create(&this->pYrCompiler), "Failed to create yara compiler.");
	REQUIRE(yr_compiler_add_string(this->pYrCompiler, yara_get_render_time_base_function.c_str(), NULL), "Failed to compile yara rule");
	REQUIRE(yr_compiler_get_rules(this->pYrCompiler, &this->rules), "Failed to get yara rules");
	POST();
}

void YaraHelper::performScan()
{
	PRE();
	yr_rules_scan_mem(this->rules, static_cast<uint8_t*>(this->moduleInfo.lpBaseOfDll), this->moduleInfo.SizeOfImage, 0, YaraHelper::callback_function, static_cast<void*>(this), 0);

	std::unique_lock<std::mutex> lk(this->mxScan);
	while (!this->isScanFinished) {
		this->cvScan.wait(lk);
	}
	POST();
}

void * YaraHelper::getPtr_getRenderTimeBase()
{
	return this->_ptr_yara_get_render_time_base_function;
}

int YaraHelper::callback_function(int message, void * message_data, void * user_data)
{
	PRE();
	YaraHelper* pThis = static_cast<YaraHelper*>(user_data);
	YR_RULE* pRule;

	switch (message) {
	case CALLBACK_MSG_RULE_NOT_MATCHING:
		pRule = static_cast<YR_RULE*>(message_data);
		LOG(LL_NFO, "CALLBACK_MSG_RULE_NOT_MATCHING: ", pRule->identifier);
		break;
	case CALLBACK_MSG_RULE_MATCHING:
		pRule = static_cast<YR_RULE*>(message_data);
		LOG(LL_NFO, "CALLBACK_MSG_RULE_MATCHING: ", pRule->identifier);
		if (std::string("yara_get_render_time_base_function").compare(pRule->identifier) == 0) {
			YR_STRING* pString;
			yr_rule_strings_foreach(pRule, pString) {
				LOG(LL_DBG, pString->identifier, "(Matches found:", pString->matches->count, ")");
				if (pString->matches->count > 1) {
					pThis->_ptr_yara_get_render_time_base_function = NULL;
					POST();
					return 0;
				}

				YR_MATCH* pMatch;
				yr_string_matches_foreach(pString, pMatch) {
					LOG(LL_DBG, " b:", (void*)pMatch->base, " o:", (void*)pMatch->offset);
					pThis->_ptr_yara_get_render_time_base_function = reinterpret_cast<void*>(pMatch->offset + (int64_t)pThis->moduleInfo.lpBaseOfDll);
					POST();
					return 0;
				}

			}
		}

		break;
	case CALLBACK_MSG_SCAN_FINISHED:
		std::lock_guard<std::mutex> guard(pThis->mxScan);
		pThis->isScanFinished = true;
		pThis->cvScan.notify_all();
		POST();
		return 0;
		break;
	}
	POST();
	return 0;
}
