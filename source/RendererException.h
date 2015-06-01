#include <string>
#include <assert.h>
enum RendererExceptionType
{
	FAIL,
	OUT_OF_MEMORY
};

class RendererException
{
public:
	RendererException()
	{
#if DEBUG
		assert(false); // Break on first failure
#endif
	}

	RendererException(std::string ErrorMessage) :
		m_ErrorMessage(ErrorMessage) {}

	std::string GetErrorMessage() { return m_ErrorMessage; }
private:
	std::string m_ErrorMessage;
};

class OutOfMemoryRendererException : public RendererException {
public:
	OutOfMemoryRendererException(std::string ErrorMessage) : RendererException(ErrorMessage) {}
};
class UnrecoverableRendererException : public RendererException {
public: 
	UnrecoverableRendererException(std::string ErrorMessage) : RendererException(ErrorMessage) {}
};

#define ORIG_CHK(expression, ExceptionType, ErrorMessage ) if(expression) { throw new ExceptionType(ErrorMessage); }
#define FAIL_CHK(expression, ErrorMessage ) ORIG_CHK(expression, UnrecoverableRendererException, ErrorMessage )
#define MEM_CHK(pointer) ORIG_CHK(!pointer, OutOfMemoryRendererException, "Failed new operator" )