import cv2


def extract_orb(gray, max_features: int = 2000):
    orb = cv2.ORB_create(nfeatures=max_features)
    keypoints, descriptors = orb.detectAndCompute(gray, None)
    return keypoints or [], descriptors
