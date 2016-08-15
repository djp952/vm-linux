;//-----------------------------------------------------------------------------
;// Copyright (c) 2016 Michael G. Brehm
;// 
;// Permission is hereby granted, free of charge, to any person obtaining a copy
;// of this software and associated documentation files (the "Software"), to deal
;// in the Software without restriction, including without limitation the rights
;// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
;// copies of the Software, and to permit persons to whom the Software is
;// furnished to do so, subject to the following conditions:
;// 
;// The above copyright notice and this permission notice shall be included in all
;// copies or substantial portions of the Software.
;// 
;// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
;// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
;// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
;// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
;// SOFTWARE.
;//-----------------------------------------------------------------------------

;//--------------------------------------------------------------------------
;// Facility Codes
FacilityNames=(
			Generic=0:FACILITY_GENERIC
			Instance=1:FACILITY_INSTANCE
			)

;//--------------------------------------------------------------------------
;// Language Codes
LanguageNames=(English=0x0409:MSG00409)

;//--------------------------------------------------------------------------
;// Error Definitions
;//--------------------------------------------------------------------------

MessageIdTypedef=HRESULT

;//--------------------------------------------------------------------------
;// Generic

MessageId=1
;//ExceptionName=ArgumentNullException,paramname
Severity=Error
Facility=Generic
SymbolicName=E_ARGUMENTNULL
Language=English
Parameter %1 value cannot be null.
.

MessageId=
;//ExceptionName=ArgumentOutOfRangeException,paramname
Severity=Error
Facility=Generic
SymbolicName=E_ARGUMENTOUTOFRANGE
Language=English
Parameter %1 was out of the range of valid values.
.

;//--------------------------------------------------------------------------
;// Instance

MessageId=1
;//ExceptionName=CreateSystemLogException,errormessage
Severity=Error
Facility=Instance
SymbolicName=E_CREATESYSTEMLOG
Language=English
Could not create the system log instance. %1!S!
.

MessageId=
;//ExceptionName=CreateJobObjectException,errorcode,errormessage
Severity=Error
Facility=Instance
SymbolicName=E_CREATEJOBOBJECT
Language=English
Could not create process control job object. Error %1!d! - %2!S!
.

MessageId=
;//ExceptionName=CreateRootNamespaceException,errormessage
Severity=Error
Facility=Instance
SymbolicName=E_CREATEROOTNAMESPACE
Language=English
Could not create the root namespace instance. %1!S!
.

MessageId=
;//ExceptionName=PanicDuringInitializationException,errormessage
Severity=Error
Facility=Instance
SymbolicName=E_PANICDURINGINITIALIZATION
Language=English
PANIC: An exception occurred during instance initialization. %1!S!
.
