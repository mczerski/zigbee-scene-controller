#define ZB_HA_DECLARE_MY_DEVICE_CLUSTER_LIST(                                   \
      cluster_list_name,                                                        \
      basic_attr_list,                                                          \
      identify_attr_list,                                                       \
      power_config_attr_list)                                                   \
      zb_zcl_cluster_desc_t cluster_list_name[] =                               \
      {                                                                         \
          ZB_ZCL_CLUSTER_DESC(                                                  \
              ZB_ZCL_CLUSTER_ID_IDENTIFY,                                       \
              ZB_ZCL_ARRAY_SIZE(identify_attr_list, zb_zcl_attr_t),             \
              (identify_attr_list),                                             \
              ZB_ZCL_CLUSTER_SERVER_ROLE,                                       \
              ZB_ZCL_MANUF_CODE_INVALID                                         \
          ),                                                                    \
          ZB_ZCL_CLUSTER_DESC(                                                  \
              ZB_ZCL_CLUSTER_ID_BASIC,                                          \
              ZB_ZCL_ARRAY_SIZE(basic_attr_list, zb_zcl_attr_t),                \
              (basic_attr_list),                                                \
              ZB_ZCL_CLUSTER_SERVER_ROLE,                                       \
              ZB_ZCL_MANUF_CODE_INVALID                                         \
          ),                                                                    \
          ZB_ZCL_CLUSTER_DESC(                                                  \
              ZB_ZCL_CLUSTER_ID_POWER_CONFIG,                                   \
              ZB_ZCL_ARRAY_SIZE(power_config_attr_list, zb_zcl_attr_t),         \
              (power_config_attr_list),                                         \
              ZB_ZCL_CLUSTER_SERVER_ROLE,                                       \
              ZB_ZCL_MANUF_CODE_INVALID                                         \
          ),                                                                    \
          ZB_ZCL_CLUSTER_DESC(                                                  \
              ZB_ZCL_CLUSTER_ID_SCENES,                                         \
              0,                                                                \
              NULL,                                                             \
              ZB_ZCL_CLUSTER_CLIENT_ROLE,                                       \
              ZB_ZCL_MANUF_CODE_INVALID                                         \
          ),                                                                    \
          ZB_ZCL_CLUSTER_DESC(                                                  \
              ZB_ZCL_CLUSTER_ID_IDENTIFY,                                       \
              0,                                                                \
              NULL,                                                             \
              ZB_ZCL_CLUSTER_CLIENT_ROLE,                                       \
              ZB_ZCL_MANUF_CODE_INVALID                                         \
          ),                                                                    \
          ZB_ZCL_CLUSTER_DESC(                                                  \
              ZB_ZCL_CLUSTER_ID_GROUPS,                                         \
              0,                                                                \
              NULL,                                                             \
              ZB_ZCL_CLUSTER_CLIENT_ROLE,                                       \
              ZB_ZCL_MANUF_CODE_INVALID                                         \
          )                                                                     \
    }

#define ZB_ZCL_DECLARE_MY_DEVICE_SIMPLE_DESC(ep_name, ep_id, in_clust_num, out_clust_num)     \
    ZB_DECLARE_SIMPLE_DESC(in_clust_num, out_clust_num);                                      \
    ZB_AF_SIMPLE_DESC_TYPE(in_clust_num, out_clust_num) simple_desc_##ep_name =               \
    {                                                                                         \
        ep_id,                                                                                \
        ZB_AF_HA_PROFILE_ID,                                                                  \
        ZB_HA_SCENE_SELECTOR_DEVICE_ID,                                                       \
        ZB_HA_DEVICE_VER_SCENE_SELECTOR,                                                      \
        0,                                                                                    \
        in_clust_num,                                                                         \
        out_clust_num,                                                                        \
        {                                                                                     \
            ZB_ZCL_CLUSTER_ID_IDENTIFY,                                                       \
            ZB_ZCL_CLUSTER_ID_BASIC,                                                          \
            ZB_ZCL_CLUSTER_ID_POWER_CONFIG,                                                   \
            ZB_ZCL_CLUSTER_ID_SCENES,                                                         \
            ZB_ZCL_CLUSTER_ID_IDENTIFY,                                                       \
            ZB_ZCL_CLUSTER_ID_GROUPS,                                                         \
        }                                                                                     \
    }

#define ZB_HA_MY_DEVICE_IN_CLUSTER_NUM 3
#define ZB_HA_MY_DEVICE_OUT_CLUSTER_NUM 3
#define ZB_HA_MY_DEVICE_REPORT_ATTR_COUNT 2

#define ZB_HA_DECLARE_MY_DEVICE_EP(ep_name, ep_id, cluster_list)     \
  ZB_ZCL_DECLARE_MY_DEVICE_SIMPLE_DESC(                              \
      ep_name,                                                       \
      ep_id,                                                         \
      ZB_HA_MY_DEVICE_IN_CLUSTER_NUM,                                \
      ZB_HA_MY_DEVICE_OUT_CLUSTER_NUM);                              \
  ZBOSS_DEVICE_DECLARE_REPORTING_CTX(                                \
      reporting_info## ep_name,                                      \
      ZB_HA_MY_DEVICE_REPORT_ATTR_COUNT);                            \
  ZB_AF_DECLARE_ENDPOINT_DESC(ep_name,                               \
                              ep_id,                                 \
      ZB_AF_HA_PROFILE_ID,                                           \
      0,                                                             \
      NULL,                                                          \
      ZB_ZCL_ARRAY_SIZE(cluster_list, zb_zcl_cluster_desc_t),        \
      cluster_list,                                                  \
      (zb_af_simple_desc_1_1_t*)&simple_desc_##ep_name,              \
      ZB_HA_MY_DEVICE_REPORT_ATTR_COUNT, reporting_info## ep_name,   \
      0, NULL) /* No CVC ctx */

#define ZB_HA_DECLARE_MY_DEVICE_CTX(device_ctx, ep_name) \
  ZBOSS_DECLARE_DEVICE_CTX_1_EP(device_ctx, ep_name)
