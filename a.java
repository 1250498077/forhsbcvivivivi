

public boolean deleteIssue(String issueKey) {
    try {
        String url = String.format(
            "%s/rest/api/2/issue/%s",
            JIRA_BASE_URL,
            issueKey
        );
        
        HttpResponse response = httpClient.httpRequest(
            url, 
            method: "DELETE", 
            jsonPayload: null
        );
        
        if (response.code() == 204) {  // 204 = No Content (成功删除)
            log.info("Successfully deleted issue: {}", issueKey);
            return true;
        } else {
            log.error("Failed to delete issue: {} - Status: {}", 
                issueKey, response.code());
            return false;
        }
        
    } catch (Exception e) {
        log.error("Error deleting issue: {}", issueKey, e);
        return false;
    }
}




/ 辅助方法：从 Sprint 字符串中提取 ID
private Integer extractSprintId(String sprintStr) {
    try {
        // 方法1：使用正则表达式提取 id= 后面的数字
        Pattern pattern = Pattern.compile("id=(\\d+)");
        Matcher matcher = pattern.matcher(sprintStr);
        
        if (matcher.find()) {
            return Integer.parseInt(matcher.group(1));
        }
        
        // 方法2：如果没找到 id=，尝试提取 @ 后面的数字（不太可靠）
        // Sprint@76a142e1 这种格式
        
    } catch (Exception e) {
        log.error("Failed to extract sprint ID from: {}", sprintStr, e);
    }
    
    return null;
}


JsonElement sprintElement = entry.getValue();
            
            if (sprintElement.isJsonArray()) {
                JsonArray sprintArray = sprintElement.getAsJsonArray();
                List<Integer> sprintIds = new ArrayList<>();
                
                for (JsonElement element : sprintArray) {
                    if (element.isJsonPrimitive() && element.getAsJsonPrimitive().isString()) {
                        String sprintStr = element.getAsString();
                        
                        // 从字符串中提取 ID
                        // 格式: "...Sprint@xxx[...id=12345,..."
                        Integer sprintId = extractSprintId(sprintStr);
                        
                        if (sprintId != null) {
                            sprintIds.add(sprintId);
                            log.info("Extracted sprint ID: {}", sprintId);
                        }
                    }
                }
                
                if (!sprintIds.isEmpty()) {
                    newFields1.put(fieldKey, sprintIds);
                }
            }



Map<String, Object> newFields1 = new HashMap<>();

// 定义需要提取 ID 的字段
Set<String> fieldsNeedIdExtraction = Set.of(
    "customfield_10720"  // Sprint 字段
    // 如果有其他类似字段，在这里添加
);

for (Map.Entry<String, JsonElement> entry : fields.entrySet()) {
    String fieldKey = entry.getKey();
    
    if (fieldKey.startsWith("customfield_") && !entry.getValue().isJsonNull()) {
        
        // 需要提取 ID 的字段
        if (fieldsNeedIdExtraction.contains(fieldKey)) {
            JsonElement element = entry.getValue();
            
            if (element.isJsonArray()) {
                JsonArray array = element.getAsJsonArray();
                List<Integer> ids = new ArrayList<>();
                
                for (JsonElement item : array) {
                    if (item.isJsonObject()) {
                        JsonObject obj = item.getAsJsonObject();
                        if (obj.has("id")) {
                            ids.add(obj.get("id").getAsInt());
                        }
                    }
                }
                
                if (!ids.isEmpty()) {
                    newFields1.put(fieldKey, ids);
                }
            } else if (element.isJsonObject()) {
                // 如果是单个对象而不是数组
                JsonObject obj = element.getAsJsonObject();
                if (obj.has("id")) {
                    newFields1.put(fieldKey, obj.get("id").getAsInt());
                }
            }
        } 
        // 其他自定义字段正常处理
        else {
            try {
                Object value = gson.fromJson(entry.getValue(), Object.class);
                newFields1.put(fieldKey, value);
            } catch (Exception e) {
                log.warn("Failed to process field {}: {}", fieldKey, e.getMessage());
            }
        }
    }
}


public Set<String> getCreatableCustomFields(String projectKey, String issueType) {
    try {
        // URL 编码
        String encodedIssueType = URLEncoder.encode(issueType, StandardCharsets.UTF_8);
        
        // 注意：Java 中使用 String.format，不是 Groovy 的 ${} 语法
        String metaUrl = String.format(
            "https://wpb-jira.systems.uk.hsbc/rest/api/2/issue/createmeta?projectKeys=%s&issuetypeNames=%s&expand=projects.issuetypes.fields",
            projectKey, 
            encodedIssueType
        );
        
        HttpResponse httpResponse = httpClient.httpRequest(metaUrl, "GET", null);
        
        // 使用 JsonParser 解析（不是 JsonSlurper）
        JsonObject meta = JsonParser.parseString(httpResponse.getText()).getAsJsonObject();
        
        // 获取 projects 数组
        JsonArray projects = meta.getAsJsonArray("projects");
        if (projects == null || projects.size() == 0) {
            System.out.println("No projects found in createmeta");
            return new HashSet<>();
        }
        
        // 获取第一个 project
        JsonObject project = projects.get(0).getAsJsonObject();
        JsonArray issuetypes = project.getAsJsonArray("issuetypes");
        if (issuetypes == null || issuetypes.size() == 0) {
            System.out.println("No issue types found");
            return new HashSet<>();
        }
        
        // 获取第一个 issuetype 的 fields
        JsonObject issuetype = issuetypes.get(0).getAsJsonObject();
        JsonObject fields = issuetype.getAsJsonObject("fields");
        if (fields == null) {
            System.out.println("No fields found");
            return new HashSet<>();
        }
        
        // 过滤出所有 customfield_ 开头的字段
        Set<String> creatableFields = new HashSet<>();
        for (String fieldKey : fields.keySet()) {
            if (fieldKey.startsWith("customfield_")) {
                creatableFields.add(fieldKey);
            }
        }
        
        System.out.println("Found " + creatableFields.size() + " creatable custom fields");
        return creatableFields;
        
    } catch (Exception e) {
        System.err.println("Error getting creatable fields: " + e.getMessage());
        e.printStackTrace();
        return new HashSet<>();
    }
}
public String createStoryFromTemplate(String templateStoryId, String newSummary, String newDescription) {
    try {
        // 1. 获取模板 story（使用 issue API）
        String getUrl = String.format("%s/rest/api/2/issue/%s", jiraBaseUrl, templateStoryId);
        HttpResponse response = httpClient.httpRequest(getUrl, method: "GET", jsonPayload: null);
        
        JsonObject issue = JsonParser.parseString(response.getText()).getAsJsonObject();
        JsonObject fields = issue.getAsJsonObject("fields");
        
        // 2. 构建新 story 的字段
        Map<String, Object> newFields = new HashMap<>();
        Gson gson = new Gson();
        
        // 必需字段
        Map<String, String> project = new HashMap<>();
        project.put("key", fields.getAsJsonObject("project").get("key").getAsString());
        newFields.put("project", project);
        
        Map<String, String> issuetype = new HashMap<>();
        issuetype.put("name", fields.getAsJsonObject("issuetype").get("name").getAsString());
        newFields.put("issuetype", issuetype);
        
        // 修改的字段
        newFields.put("summary", newSummary);
        if (newDescription != null) {
            newFields.put("description", newDescription);
        } else if (fields.has("description") && !fields.get("description").isJsonNull()) {
            newFields.put("description", fields.get("description").getAsString());
        }
        
        // 复制其他字段
        List<String> fieldsToCopy = Arrays.asList("priority", "assignee", "labels", "components");
        for (String fieldKey : fieldsToCopy) {
            if (fields.has(fieldKey) && !fields.get(fieldKey).isJsonNull()) {
                Object value = gson.fromJson(fields.get(fieldKey), Object.class);
                newFields.put(fieldKey, value);
            }
        }
        
        // 复制自定义字段（排除只读字段）
        for (Map.Entry<String, JsonElement> entry : fields.entrySet()) {
            String fieldKey = entry.getKey();
            if (fieldKey.startsWith("customfield_") && !entry.getValue().isJsonNull() && !isReadOnlyField(fieldKey)) {
                Object value = gson.fromJson(entry.getValue(), Object.class);
                newFields.put(fieldKey, value);
            }
        }
        
        // 3. 创建新 story
        Map<String, Object> payload = new HashMap<>();
        payload.put("fields", newFields);
        
        String createUrl = String.format("%s/rest/api/2/issue/", jiraBaseUrl);
        String requestBody = gson.toJson(payload);
        
        HttpResponse createResponse = httpClient.httpRequest(createUrl, method: "POST", jsonPayload: requestBody);
        JsonObject createdIssue = JsonParser.parseString(createResponse.getText()).getAsJsonObject();
        
        return createdIssue.get("key").getAsString();
        
    } catch (Exception e) {
        throw new RuntimeException("Failed to create story: " + e.getMessage(), e);
    }
}



private boolean isReadOnlyField(String fieldKey) {
    // 某些自定义字段是系统生成的，不能在创建时设置
    List<String> readOnlyFields = [
        "customfield_12077",  // 例如：自动计算的字段
        "archiveddate"
    ];
    return readOnlyFields.contains(fieldKey);
}
