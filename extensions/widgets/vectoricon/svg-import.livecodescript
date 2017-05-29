script "com.livecode.vectoricon.svg-import"
local sSpec

command svgImportFromFile pXMLFile, @rVGLText
   svgSpecLoad
   
   local tContext
   put revCreateXMLTreeFromFile(pXMLFile, true, true, false) into tContext["tree"]  #wellFormed, createTree, sendMessages
   if tContext["tree"] begins with "xmlerr," then
      throw "svgerr," & item 2 to -1 of tContext["tree"]
   end if
   
   put empty into tContext["node-stack"]
   put empty into tContext["attribute-stack"]
   put empty into tContext["vgl"]
   
   --try
   svgParseNode tContext, revXMLRootNode(tContext["tree"])
   put tContext["vgl"] into rVGLText
   --finally
   revDeleteXMLTree tContext["tree"]
   --end try
end svgImportFromFile

----------

private command svgParseNode @xContext, pNode
   seqPushOntoBack xContext["node-stack"], svgCurrentNode(xContext) & slash & pNode
   seqPushOntoBack xContext["attribute-stack"], empty
   
   svgParseAttributes xContext
   
   svgParseDispatch xContext, "before"
   
   local tChild
   repeat for each item tChild in svgCurrentChildren(xContext)
      svgParseNode xContext, tChild
   end repeat
   
   svgParseDispatch xContext, "after"
   
   seqPopFromBack xContext["attribute-stack"]
   seqPopFromBack xContext["node-stack"]
end svgParseNode

private command svgParseDispatch @xContext, pPhase
   switch svgCurrentNodeType(xContext)
      case "svg"
         svgParseDispatchSvg xContext, pPhase
         break
      case "g"
         svgParseDispatchG xContext, pPhase
         break
      case "path"
         svgParseDispatchPath xContext, pPhase
         break
   end switch
end svgParseDispatch

private command svgParseDispatchSvg @xContext, pPhase
   
end svgParseDispatchSvg

private command svgParseDispatchG @xContext, pPhase
   if pPhase is "before" then
      svgEmit xContext, "B"
   else if pPhase is "after" then
      svgEmit xContext, "E"
   end if
end svgParseDispatchG

private command svgParseDispatchPath @xContext, pPhase
   
end svgParseDispatchPath

private command svgParseAttributes @xContext
   -- Fetch an array of XML node attributes
   local tXMLAttributes, tCSSAttributes
   put svgParseXMLAttributes(xContext) into tXMLAttributes
   
   -- If the node has a style attribute, parse it as CSS
   if "style" is among the keys of tXMLAttributes then
      put svgParseCSSAttributes(xContext, tXMLAttributes["style"]) into tCSSAttributes
      delete variable tXMLAttributes["style"]
   end if
   
   -- Prefer CSS over XML attributes
   local tAttributes
   put tCSSAttributes into tAttributes
   union tAttributes with tXMLAttributes
   
   -- Push attributes onto stack
   repeat for each key tAttr in sSpec["attribute"]
      svgApplyAttribute xContext, tAttr, tAttributes[tAttr]
   end repeat
end svgParseAttributes

private command svgApplyAttribute @xContext, pAttr, pValue
   if pAttr is not among the keys of sSpec["attribute"] then
      log "*** unknown attribute -" && pAttr && " ***"
      exit svgApplyAttribute
   end if
   
   local tResolvedValue
   if pValue is empty then
      put empty into tResolvedValue
   else
      local tAttrValues
      put sSpec["attribute"][pAttr]["value"] into tAttrValues
      repeat for each element tValueType in tAttrValues
         if tValueType is "inherit" then
            if pValue is "inherit" then
               put svgFetchParentAttribute(xContext, pAttr) into tResolvedValue
               exit repeat
            end if
         else if tValueType is pValue then
            put pValue into tResolvedValue
            exit repeat
         else
            put svgParseCustomAttributeValue(xContext, tValueType, pValue) into tResolvedValue
         end if
      end repeat
   end if
   
   if tResolvedValue is empty then
      if sSpec["attribute"][pAttr]["inheritable"] then
         put svgFetchParentAttribute(xContext, pAttr) into tResolvedValue
      else
         put sSpec["attribute"][pAttr]["initial"] into tResolvedValue
      end if
   end if
   
   if tResolvedValue is empty then
      log "ignoring attribute value " & pValue & " for attr " & pAttr
      exit svgApplyAttribute
   end if
   
   put tResolvedValue into xContext["attribute-stack"][the number of elements in xContext["attribute-stack"]][pAttr]
end svgApplyAttribute

private function svgParseCustomAttributeValue @xContext, pValueType, pValue
   switch pValueType
      case "<paint>"
         return "paint value"
      case "<opacity-value>"
         if pValue is a number then
            return max(min(pValue, 1), 0)
         else
            return empty
         end if
         break
      case "<length>"
         if pValue is a number then
            return pValue
         else
            return empty
         end if
         break
      case "<dash-array>"
         return "dash array value"
         break
      default
         break
   end switch
   
   if pValueType begins with "ignore(" then
      return "ignore() value"
   end if
   
   log "ignoring attribute value " & pValue & " as unknown type" && pValueType
   return empty
end svgParseCustomAttributeValue

private function svgFetchParentAttribute @xContext, pAttr
   if the number of elements in xContext["attribute-stack"] is 1 then
      return sSpec["attribute"][pAttr]["initial"]
   end if
   
   return xContext["attribute-stack"][the number of elements in xContext["attribute-stack"] - 1][pAttr]
end svgFetchParentAttribute

private function svgParseXMLAttributes pContext
   local tAttributes
   put revXMLAttributes(pContext["tree"], svgCurrentNode(pContext), ":", return) into tAttributes
   split tAttributes using return and ":"
   return tAttributes
end svgParseXMLAttributes

private function svgParseCSSAttributes pContext, pCSSAttributes
   return empty
end svgParseCSSAttributes

private function svgCurrentChildren pContext
   return revXMLChildNames(pContext["tree"], svgCurrentNode(pContext), ",", , true)
end svgCurrentChildren

private function svgCurrentNode pContext
   return seqTop(pContext["node-stack"])
end svgCurrentNode

private function svgCurrentNodeType pContext
   set the itemDelimiter to slash
   get the last item of svgCurrentNode(pContext)
   set the itemDelimiter to "["
   return the first item of it
end svgCurrentNodeType

private command svgEmit @xContext, pCommand
   put pCommand & return after xContext["vgl"]
end svgEmit

----------

private function resourcesPath
   set the itemDelimiter to slash
   return item 1 to -2 of the filename of this stack
end resourcesPath

private command svgSpecLoad
   local tSpecText
   put url ("file:" & resourcesPath() & slash & "svg-specification.txt") into tSpecText
   
   local tSpec
   put empty into tSpec
   
   local tCurrentType, tCurrentName
   repeat for each line tLine in tSpecText
      if word 1 of tLine begins with "#" then
         next repeat
      end if
      if word 1 of tLine is "attribute" then
         put "attribute" into tCurrentType
         put word 2 of tLine into tCurrentName
      else if tCurrentType is "attribute" then
         switch word 1 of tLine
            case "value"
               put svgSpecParseValue(word 2 to -1 of tLine) into tSpec[tCurrentType][tCurrentName]["value"]
               break
            case "initial"
               put word 2 to -1 of tLine into tSpec[tCurrentType][tCurrentName]["initial"]
               break
            case "inherited"
               put svgSpecParseBoolean(word 2 of tLine) into tSpec[tCurrentType][tCurrentName]["inherited"]
               break
            case "animatable"
               put svgSpecParseBoolean(word 2 of tLine) into tSpec[tCurrentType][tCurrentName]["animatable"]
               break
         end switch
      end if
   end repeat
   
   put tSpec into sSpec
end svgSpecLoad

private function svgSpecParseValue pValue
   split pValue by "|"
   repeat for each key tKey in pValue
      put word 1 to -1 of pValue[tKey] into pValue[tKey]
   end repeat
   return pValue
end svgSpecParseValue

private function svgSpecParseBoolean pValue
   get word 1 to -1 of pValue
   if pValue is "yes" then
      return true
   else if pValue is "no" then
      return false
   else
      throw "*** error in spec ***"
   end if
end svgSpecParseBoolean

----------

private command seqPushOntoBack @xSeq, pElement
   put pElement into xSeq[the number of elements in xSeq + 1]
end seqPushOntoBack

private command seqPopFromBack @xSeq
   delete variable xSeq[the number of elements in xSeq]
end seqPopFromBack

private function seqTop pSeq
   return pSeq[the number of elements in pSeq]
end seqTop

----------

private command log pMessage
   put pMessage & return after field "Log" of stack "VectorIconProject"
   --set the vScroll of field "Log" of stack "VectorIconProject" to the formattedHeight of field "Log" of stack "VectorIconProject"
end log
