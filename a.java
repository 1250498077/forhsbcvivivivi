


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
